//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  Book-wide search scope implementation. See KBSBookScope.h for the overall contract.
//  Ported from KESCLBookScope (KESCL left untouched); the toggle and the jump-time
//  reopen/early-close machinery are omitted in this Step-1 subset.
//
//========================================================================================

#include "VCPlugInHeaders.h"

// Interface includes:
#include "IBook.h"
#include "IBookContent.h"
#include "IBookContentMgr.h"
#include "IBookManager.h"
#include "IBookUtils.h"			// OpenOneDocument, OriginallyCloseDocInfo
#include "IDocFileHandler.h"
#include "IDocument.h"
#include "IDocumentList.h"
#include "IDocumentUtils.h"
#include "ISession.h"

// General includes:
#include "ErrorUtils.h"			// PMSetGlobalErrorCode - a failed Open must not poison later commands
#include "PersistUtils.h"		// ::GetUIDRef / ::GetDataBase
#include "SDKFileHelper.h"
#include "Utils.h"
#include "PMString.h"
#include "WideString.h"

// Project includes:
#include "KBSBookScope.h"

namespace
{
	// The chapters WE opened (only the originally-closed ones - OpenOneDocument records exactly
	// those in here), held open so a repeat search skips the load. OpenOneDocument also uses
	// this record to stay under the open-database cap.
	OriginallyCloseDocInfo gHeldDocInfo;

	// Which book the held chapters belong to (its full file path). A search against a DIFFERENT
	// book first closes the previous book's held chapters, so switching books never piles up
	// hidden documents from every book visited.
	PMString gHeldBookPath;
}

bool KBSBookScope::IsDocStillOpen(const UIDRef& docRef)
{
	InterfacePtr<IDocumentList> docList(GetExecutionContextSession()->QueryDocumentList());
	if (docList == nil)
		return false;
	const int32 count = docList->GetDocCount();
	for (int32 i = 0; i < count; ++i)
	{
		IDocument* doc = docList->GetNthDoc(i);
		if (doc != nil && ::GetUIDRef(doc) == docRef)
			return true;
	}
	return false;
}

void KBSBookScope::ReleaseHeldDocs()
{
	if (gHeldDocInfo.fCurrentOpenedDocumentList.size() == 0)
	{
		gHeldBookPath.Clear();
		return;
	}

	// Take the list first, so a re-entrant call finds it already empty instead of scheduling
	// the closes twice.
	K2Vector<UIDRef> held;
	held = gHeldDocInfo.fCurrentOpenedDocumentList;
	gHeldDocInfo.Clear();
	gHeldBookPath.Clear();

	// Close each chapter through the stock document close. kSchedule defers each close until the
	// current notification / idle tick has unwound; kSuppressUI + the search-time dirty guard =
	// no save prompt. Skip chapters the user closed already (a dead UIDRef must not reach the
	// close machinery).
	for (int32 i = 0; i < static_cast<int32>(held.size()); ++i)
	{
		if (!IsDocStillOpen(held[i]))
			continue;
		InterfacePtr<IDocFileHandler> docFileHandler(Utils<IDocumentUtils>()->QueryDocFileHandler(held[i]));
		if (docFileHandler == nil)
			continue;
		if (docFileHandler->CanClose(held[i]))
			docFileHandler->Close(held[i], kSuppressUI, kFalse /*allowCancel*/, IDocFileHandler::kSchedule);
	}
}

void KBSBookScope::ShutdownCleanup()
{
	// State only, no closing, no UI: this runs while InDesign is tearing down. Clear() releases
	// the vector's storage too, so the static destructor at DLL unload finds nothing to do.
	gHeldDocInfo.Clear();
	gHeldBookPath.Clear();
}

bool KBSBookScope::HasActiveBook()
{
	InterfacePtr<IBookManager> bookMgr(GetExecutionContextSession(), UseDefaultIID());
	if (bookMgr == nil)
		return false;
	return bookMgr->GetCurrentActiveBook() != nil;	// non-owning pointer - no release
}

bool KBSBookScope::GetBookChapterDocs(std::vector<ChapterDoc>& outDocs, PMString& outBookName)
{
	outDocs.clear();
	outBookName.Clear();

	InterfacePtr<IBookManager> bookMgr(GetExecutionContextSession(), UseDefaultIID());
	if (bookMgr == nil)
		return false;

	// The one active book (the front-most open book panel). GetCurrentActiveBook hands out a
	// non-owning pointer - no release.
	IBook* book = bookMgr->GetCurrentActiveBook();
	if (book == nil)
		return false;

	outBookName = book->GetBookTitleName();
	outBookName.SetTranslatable(kFalse);

	IDataBase* bookDB = ::GetDataBase(book);
	if (bookDB == nil)
		return false;

	// A different book than the chapters we hold were opened for? Close those first, then
	// remember the new book as the held one.
	SDKFileHelper bookFileHelper(book->GetBookFileSpec());
	const PMString bookPath = bookFileHelper.GetPath();
	if (!(gHeldBookPath == bookPath))
	{
		ReleaseHeldDocs();
		gHeldBookPath = bookPath;
	}

	InterfacePtr<IBookContentMgr> contentMgr(book, UseDefaultIID());
	if (contentMgr == nil)
		return false;

	const int32 contentCount = contentMgr->GetContentCount();
	for (int32 i = 0; i < contentCount; ++i)
	{
		const UID contentUID = contentMgr->GetNthContent(i);
		if (contentUID == kInvalidUID)
			continue;

		InterfacePtr<IBookContent> content(bookDB, contentUID, UseDefaultIID());
		if (content == nil)
			continue;

		// Open the chapter's document without a layout window (or reuse it if it is already
		// open). Chapters that cannot be opened - missing file, document locked by another user -
		// are skipped; the searchable rest is still worth having. gHeldDocInfo accumulates
		// whatever this call had to open.
		UIDRef docRef;
		if (Utils<IBookUtils>()->OpenOneDocument(bookDB, contentUID, docRef, gHeldDocInfo) != kSuccess)
			continue;
		if (docRef == UIDRef::gNull)
			continue;

		ChapterDoc chapter;
		chapter.docRef = docRef;
		// The chapter's .indd, so navigation can reopen the chapter if the user closes it.
		content->GetIDFile(chapter.file);
		// The chapter's file name for the read-out. Via the UTF-16 buffer (AppendW), so a
		// Japanese chapter name survives - the PMString(char*) conversions do not.
		WideString shortName = content->GetShortName();
		chapter.shortName.SetTranslatable(kFalse);
		const UTF16TextChar* buf = shortName.GrabUTF16Buffer(nil);
		if (buf != nil)
			chapter.shortName.AppendW(buf);
		outDocs.push_back(chapter);
	}

	return !outDocs.empty();
}

// End, KBSBookScope.cpp.
