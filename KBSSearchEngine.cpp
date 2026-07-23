//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  Search engine implementation. See KBSSearchEngine.h for the contract. The walker loop is
//  ported from KESCL's CollectMatches (KESCL left untouched); the one change is the point of
//  this plugin: KBS does NOT set the find string or pin the search mode - it walks with the
//  user's current Find/Change options as they are, so the mode (Text or GREP) is followed.
//
//  For each match the finder hands back (story, start, end); BuildHit then reads the containing
//  paragraph (IComposeScanner::FindSurroundingParagraph + CopyText) and splits it, at the exact
//  UTF-16 offsets, into the three segments the colour cell paints. The split is UTF-16-unit
//  exact (GrabUTF16Buffer + SetXString) so it matches TextIndex semantics; match boundaries
//  never fall inside a surrogate pair, so no code point is ever cut.
//
//========================================================================================

#include "VCPlugInHeaders.h"

// Interface includes:
#include "IComposeScanner.h"		// FindSurroundingParagraph / CopyText (the hit line's text)
#include "IDocument.h"
#include "ILayoutUIUtils.h"
#include "IFindChangeOptions.h"
#include "IFindChangeCmdData.h"
#include "IFindChangeService.h"		// FindChangeResult enum
#include "ICommand.h"
#include "IK2ServiceProvider.h"
#include "IK2ServiceRegistry.h"
#include "ITextModel.h"
#include "ITextWalker.h"			// also declares ITextWalkerClient
#include "ITextWalkerScope.h"
#include "ITextWalkerSelectionUtils.h"	// TextWalkerSelections_CriticalSection
#include "IWalkerScopeFactoryUtils.h"
#include "ISession.h"				// GetExecutionContextSession
// For naming the page a match sits on (the "P<page>(<n>)" hit-row locator):
#include "ITextParcelList.h"		// GetParcelContaining - the parcel a position is in
#include "IParcelList.h"			// GetParcelFrameUID - is that parcel placed in a frame?
#include "ParcelKey.h"				// ParcelKey::IsValid
#include "IHierarchy.h"				// the parcel frame as a page item
#include "ILayoutUtils.h"			// GetOwnerPageUID - frame -> page
#include "IPageList.h"				// GetPageString / GetPageIndex - page -> "12" / "A:1"

// General includes:
#include "TextWalkerServiceProviderID.h"	// kFindTextCmdBoss, kFindChangeClientBoss, kTextWalkerService(...)
#include "WalkerScopeOptions.h"
#include "ErrorUtils.h"				// PMSetGlobalErrorCode
#include "CmdUtils.h"
#include "CreateObject.h"
#include "PreferenceUtils.h"		// QuerySessionPreferences
#include "PersistUtils.h"			// ::GetUIDRef
#include "IDataBase.h"				// SaveRestoreModifiedState
#include "Utils.h"
#include "WideString.h"

#include <vector>
#include <algorithm>				// std::stable_sort (the matches' page order)

// Project includes:
#include "KBSSearchEngine.h"
#include "KBSBookScope.h"
#include "KBSResultModel.h"

namespace
{

// Is there any text to find on the Find/Change panel right now (in the current mode)?
bool HasFindQuery()
{
	InterfacePtr<IFindChangeOptions> opts(QuerySessionPreferences<IFindChangeOptions>());
	if (opts == nil)
		return false;
	// The find-what for the mode the user is actually in (Text vs GREP each have their own).
	const WideString& findText = opts->GetFindString(opts->GetSearchMode());
	return !findText.empty();
}

// The page a match position sits on, named the way the Pages panel names it (section prefix
// and all). Ported from KESCL's GetMatchPageString: position -> parcel -> frame -> page. Returns
// false for an overset position (composed but placed in no frame, so no page) and for the query
// failures around it, which read the same to the user: no page to name. outPageIndex is the
// page's plain document order (for sorting; the STRING can be "iv" / "A-1" under a section).
bool GetMatchPageString(const UIDRef& docRef, const UIDRef& storyRef, TextIndex pos,
	PMString& outPage, int32& outPageIndex)
{
	outPage.Clear();
	outPage.SetTranslatable(kFalse);
	outPageIndex = -1;

	InterfacePtr<ITextModel> textModel(storyRef, UseDefaultIID());
	if (textModel == nil)
		return false;
	InterfacePtr<ITextParcelList> tpl(textModel->QueryTextParcelList(pos));
	if (tpl == nil)
		return false;
	const ParcelKey key = tpl->GetParcelContaining(pos);
	if (!key.IsValid())
		return false;
	InterfacePtr<IParcelList> pl(tpl, UseDefaultIID());
	if (pl == nil)
		return false;
	const UID frameUID = pl->GetParcelFrameUID(key);
	if (frameUID == kInvalidUID)
		return false;	// overset: composed but placed in no frame

	InterfacePtr<IHierarchy> frameHier(docRef.GetDataBase(), frameUID, UseDefaultIID());
	if (frameHier == nil)
		return false;
	const UID pageUID = Utils<ILayoutUtils>()->GetOwnerPageUID(frameHier);
	if (pageUID == kInvalidUID)
		return false;

	InterfacePtr<IPageList> pageList(docRef, UseDefaultIID());
	if (pageList == nil)
		return false;
	pageList->GetPageString(pageUID, &outPage);	// defaults: section-aware, abbreviated
	outPage.SetTranslatable(kFalse);
	outPageIndex = pageList->GetPageIndex(pageUID);
	return !outPage.IsEmpty();
}

// Fill a hit from one match (story, [start, end)): its jump anchors, and the containing
// paragraph's text split into (before / matched / after) at the exact UTF-16 offsets.
void BuildHit(const UIDRef& docRef, const UIDRef& storyRef, TextIndex start, TextIndex end, KBSResultModel::Hit& outHit)
{
	outHit.storyUID = storyRef.GetUID();
	outHit.textStart = start;
	outHit.textEnd = end;

	// The page this match sits on (for the "P<page>(<n>) " hit-row locator). Empty page /
	// pageIndex -1 marks an overset match. Recomposes on demand - fine, the caller's dirty guard
	// is up.
	if (!GetMatchPageString(docRef, storyRef, start, outHit.pageString, outHit.pageIndex))
	{
		outHit.pageString.Clear();
		outHit.pageIndex = -1;
	}

	InterfacePtr<ITextModel> model(storyRef, UseDefaultIID());
	if (model == nil)
		return;
	InterfacePtr<IComposeScanner> scanner(model, UseDefaultIID());
	if (scanner == nil)
		return;

	// The paragraph that holds the match; excludeEOS (default) trims the paragraph terminator.
	int32 paraLen = 0;
	const TextIndex paraStart = scanner->FindSurroundingParagraph(start, &paraLen);
	if (paraStart < 0 || paraLen <= 0)
		return;

	WideString para;
	scanner->CopyText(paraStart, paraLen, &para);

	// Split by UTF-16 unit, matching TextIndex: the match sits at [start-paraStart, end-paraStart)
	// in the paragraph. GrabUTF16Buffer + SetXString copies exact unit ranges (a match boundary
	// never falls inside a surrogate pair, so no code point is cut).
	PMString paraStr(para);
	int32 n = 0;
	const UTF16TextChar* buf = paraStr.GrabUTF16Buffer(&n);
	if (buf == nil || n <= 0)
		return;

	int32 ms = static_cast<int32>(start - paraStart);
	int32 ml = static_cast<int32>(end - start);
	if (ms < 0)		ms = 0;
	if (ms > n)		ms = n;
	if (ml < 0)		ml = 0;
	if (ms + ml > n)	ml = n - ms;

	outHit.preText.SetXString(buf, ms);
	outHit.preText.SetTranslatable(kFalse);
	outHit.matchText.SetXString(buf + ms, ml);
	outHit.matchText.SetTranslatable(kFalse);
	outHit.postText.SetXString(buf + ms + ml, n - ms - ml);
	outHit.postText.SetTranslatable(kFalse);
}

// Walk one document with the user's current Find/Change query and collect every match as a Hit.
// Read-only: the whole walk sits inside a SaveRestoreModifiedState dirty guard, so a windowless
// chapter can be closed afterwards without wanting a save. NOTHING is set on opts - the walk uses
// the user's Find/Change settings verbatim, so the search mode (Text or GREP) is followed.
void CollectHitsInDoc(const UIDRef& docRef, std::vector<KBSResultModel::Hit>& outHits)
{
	InterfacePtr<IFindChangeOptions> opts(QuerySessionPreferences<IFindChangeOptions>());
	if (opts == nil)
		return;

	IDataBase::SaveRestoreModifiedState dirtyGuard(docRef.GetDataBase());

	InterfacePtr<IK2ServiceRegistry> registry(GetExecutionContextSession(), UseDefaultIID());
	if (registry == nil)
		return;
	InterfacePtr<IK2ServiceProvider> provider(registry->QueryServiceProviderByClassID(kTextWalkerService, kTextWalkerServiceProviderBoss));
	if (provider == nil)
		return;

	InterfacePtr<ITextWalker> walker(provider, UseDefaultIID());
	if (walker == nil)
		return;

	// Always start a fresh walk from the top of the document.
	if (walker->IsWalking())
		walker->Halt();

	// Whole active document: master pages / locked layers / locked stories / footnotes
	// (defaults), but exclude hidden layers.
	WalkerScopeOptions scopeOptions;
	scopeOptions.SetIncludeHiddenLayers(kFalse);

	InterfacePtr<ITextWalkerScope> scope(Utils<IWalkerScopeFactoryUtils>()->QueryDocumentWalkerScope(docRef, scopeOptions));
	if (scope == nil)
		return;

	InterfacePtr<ITextWalkerClient> client(static_cast<ITextWalkerClient*>(::CreateObject2<ITextWalkerClient>(kFindChangeClientBoss)));
	if (client == nil)
		return;

	walker->Initialize(client, scope, opts, nil);

	InterfacePtr<ITextWalkerSelectionUtils> selUtils(walker, UseDefaultIID());
	if (selUtils == nil)
		return;

	// Required critical section around text-walker selection changes.
	const TextWalkerSelections_CriticalSection criticalSection(selUtils);

	// Walk the whole document. Each ProcessCommand advances the walker to the next match ("find
	// next"), so we keep going until no more hits.
	while (true)
	{
		InterfacePtr<ICommand> findCmd(CmdUtils::CreateCommand(kFindTextCmdBoss));
		if (findCmd == nil)
			break;
		InterfacePtr<IFindChangeCmdData> cmdData(findCmd, UseDefaultIID());
		if (cmdData == nil)
			break;
		cmdData->SetTextWalker(walker);

		if (CmdUtils::ProcessCommand(findCmd) != kSuccess)
		{
			// End of THIS walk only: the error state a failed find raises would otherwise
			// outlive it and block every later command in the session.
			ErrorUtils::PMSetGlobalErrorCode(kSuccess);
			break;
		}
		if (cmdData->GetFindChangeResult() != IFindChangeService::kSuccess)
			break;	// kNotFound: no more matches, walk complete.

		TextIndex start = kInvalidTextIndex;
		TextIndex end = kInvalidTextIndex;
		UIDRef story = cmdData->GetRange(start, end);

		KBSResultModel::Hit hit;
		BuildHit(docRef, story, start, end, hit);
		outHits.push_back(hit);
	}

	if (walker->IsWalking())
		walker->Halt();
}

// Put a chapter's hits in PAGE order and bake the "P<page>(<n>) " locator onto each hit line
// (the KESCL convention: page string, section-aware; a within-page ordinal in parens only when
// the page holds more than one match; "ov" for an overset match, which has no page). The locator
// rides on the front of preText, so the colour cell draws it in the normal colour ahead of the
// match. Pure string / index work - no recompose, so no dirty guard needed here.
void FinalizeChapterHits(std::vector<KBSResultModel::Hit>& hits)
{
	// Page order, overset matches to the end (their pageIndex is -1). Stable, so hits on the
	// same page keep their document (walk) order.
	std::stable_sort(hits.begin(), hits.end(),
		[](const KBSResultModel::Hit& a, const KBSResultModel::Hit& b)
		{
			const int32 pa = (a.pageIndex < 0) ? kMaxInt32 : a.pageIndex;
			const int32 pb = (b.pageIndex < 0) ? kMaxInt32 : b.pageIndex;
			return pa < pb;
		});

	// Walk contiguous runs of the same page (equal pageIndex after the sort): the run length is
	// the page's match count, and the position within the run is the within-page ordinal.
	size_t i = 0;
	while (i < hits.size())
	{
		size_t j = i;
		while (j < hits.size() && hits[j].pageIndex == hits[i].pageIndex)
			++j;
		const int32 runCount = static_cast<int32>(j - i);

		for (size_t k = i; k < j; ++k)
		{
			PMString locator;
			locator.SetTranslatable(kFalse);
			if (hits[k].pageString.IsEmpty())
			{
				locator.Append("ov");	// overset: no page (lowercase, as in KESCL)
			}
			else
			{
				locator.Append("P");
				locator.Append(hits[k].pageString);
				if (runCount > 1)
				{
					const int32 ordinal = static_cast<int32>(k - i) + 1;
					locator.Append("(");
					locator.AppendNumber(ordinal);
					locator.Append(")");
				}
			}
			// The locator is its own part now (drawn at full colour, then a tab stop before the
			// line text) - the colour cell keeps it separate from the faded line segments.
			hits[k].locator = locator;
		}
		i = j;
	}
}

} // anonymous namespace

int32 KBSSearchEngine::SearchBook(PMString& outSummary)
{
	outSummary.Clear();
	outSummary.SetTranslatable(kFalse);

	KBSResultModel::Clear();

	if (!HasFindQuery())
	{
		outSummary.Append("No Find/Change text set. Type what to find in Edit > Find/Change, then Search Book.");
		return 0;
	}

	// Resolve the scope: the active book's chapters, else the front document.
	std::vector<KBSBookScope::ChapterDoc> targets;
	PMString bookName;
	bool fromBook = false;
	if (KBSBookScope::HasActiveBook())
		fromBook = KBSBookScope::GetBookChapterDocs(targets, bookName);

	if (!fromBook || targets.empty())
	{
		targets.clear();
		IDocument* doc = Utils<ILayoutUIUtils>()->GetFrontDocument();
		if (doc == nil)
		{
			outSummary.Append(KBSBookScope::HasActiveBook()
				? "The active book has no openable chapters."
				: "No active book and no open document to search.");
			return 0;
		}
		KBSBookScope::ChapterDoc single;
		single.docRef = ::GetUIDRef(doc);
		doc->GetName(single.shortName);
		single.shortName.SetTranslatable(kFalse);
		targets.push_back(single);
	}

	// Walk every target; only chapters that hold a hit go into the model (no empty branches).
	std::vector<KBSResultModel::Chapter> chapters;
	int32 total = 0;
	for (size_t i = 0; i < targets.size(); ++i)
	{
		std::vector<KBSResultModel::Hit> hits;
		CollectHitsInDoc(targets[i].docRef, hits);
		if (hits.empty())
			continue;

		// Page-order the hits and bake the "P<page>(<n>) " locator onto each line.
		FinalizeChapterHits(hits);

		KBSResultModel::Chapter chapter;
		chapter.name = targets[i].shortName;
		chapter.name.SetTranslatable(kFalse);
		chapter.docRef = targets[i].docRef;
		chapter.file = targets[i].file;
		chapter.hits.swap(hits);
		total += static_cast<int32>(chapter.hits.size());
		chapters.push_back(chapter);
	}

	KBSResultModel::SetResults(chapters);

	// Task 3: the windowless chapters stay HELD so a hit-row jump can reach them without a
	// document load. They are released only when a DIFFERENT book is searched (KBSBookScope's
	// book-switch guard) or at shutdown - a same-book re-search reuses them.

	// The one-line summary. The hit count leads, so it stays visible even when the narrow
	// single-line status field truncates the tail.
	outSummary.AppendNumber(total);
	outSummary.Append(" hit(s)");
	if (fromBook)
	{
		PMString chapStr;	chapStr.AppendNumber(static_cast<int32>(chapters.size()));
		outSummary.Append(" in ");
		outSummary.Append(chapStr);
		outSummary.Append(" chapter(s) - book \"");
		outSummary.Append(bookName);
		outSummary.Append("\".");
	}
	else
	{
		outSummary.Append(" - front document (no active book).");
	}
	return total;
}

// End, KBSSearchEngine.cpp.
