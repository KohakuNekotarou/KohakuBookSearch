//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  Result model implementation. See KBSResultModel.h for the contract. All state is a single
//  file-static vector of chapters; the getters are bounds-checked so a repaint racing a rebuild
//  (or a stale node id) reads "nothing" rather than crashing.
//
//========================================================================================

#include "VCPlugInHeaders.h"

// General includes:
#include "Utils.h"

// Project includes:
#include "KBSResultModel.h"

namespace
{
	std::vector<KBSResultModel::Chapter> gChapters;
}

void KBSResultModel::SetResults(const std::vector<Chapter>& chapters)
{
	gChapters = chapters;
}

void KBSResultModel::Clear()
{
	gChapters.clear();
}

void KBSResultModel::ShutdownCleanup()
{
	// Assigning a fresh vector releases the storage too, not just the contents, so the static
	// destructor at DLL unload finds nothing left to do (the KESCL ShutdownCleanup rule).
	gChapters = std::vector<Chapter>();
}

int32 KBSResultModel::GetChapterCount()
{
	return static_cast<int32>(gChapters.size());
}

int32 KBSResultModel::GetHitCount(int32 chapterIdx)
{
	if (chapterIdx < 0 || chapterIdx >= static_cast<int32>(gChapters.size()))
		return 0;
	return static_cast<int32>(gChapters[chapterIdx].hits.size());
}

bool KBSResultModel::GetChapterDisplay(int32 chapterIdx, PMString& outName, int32& outHitCount)
{
	if (chapterIdx < 0 || chapterIdx >= static_cast<int32>(gChapters.size()))
		return false;
	const Chapter& c = gChapters[chapterIdx];
	outName = c.name;
	outName.SetTranslatable(kFalse);
	outHitCount = static_cast<int32>(c.hits.size());
	return true;
}

bool KBSResultModel::GetHitDisplay(int32 chapterIdx, int32 hitIdx,
	PMString& outPre, PMString& outMatch, PMString& outPost)
{
	if (chapterIdx < 0 || chapterIdx >= static_cast<int32>(gChapters.size()))
		return false;
	const Chapter& c = gChapters[chapterIdx];
	if (hitIdx < 0 || hitIdx >= static_cast<int32>(c.hits.size()))
		return false;
	const Hit& h = c.hits[hitIdx];
	outPre = h.preText;
	outMatch = h.matchText;
	outPost = h.postText;
	return true;
}

bool KBSResultModel::GetHitLocation(int32 chapterIdx, int32 hitIdx,
	UIDRef& outDocRef, IDFile& outFile, UID& outStoryUID, TextIndex& outStart, TextIndex& outEnd)
{
	if (chapterIdx < 0 || chapterIdx >= static_cast<int32>(gChapters.size()))
		return false;
	const Chapter& c = gChapters[chapterIdx];
	if (hitIdx < 0 || hitIdx >= static_cast<int32>(c.hits.size()))
		return false;
	const Hit& h = c.hits[hitIdx];
	outDocRef = c.docRef;
	outFile = c.file;
	outStoryUID = h.storyUID;
	outStart = h.textStart;
	outEnd = h.textEnd;
	return true;
}

// End, KBSResultModel.cpp.
