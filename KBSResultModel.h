//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  The result model: the last book search's hits, grouped by chapter, that the result tree
//  (KBSResultListAdapter / KBSResultListWidgetMgr) displays. A tiny session-global store - the
//  KBS analog of KESCL's KESCLBatchCheck, minus its filters / reverse mode / per-value rows,
//  because the KBS tree is a flat two levels: chapter -> hit.
//
//  Each hit already carries its display text pre-split into three PMString segments (the text
//  before the match, the matched text, and the text after) so the colour cell just paints three
//  runs and no UTF-16 boundary maths happens at draw time (the split is done once, in
//  KBSSearchEngine, against the paragraph's wide string at the finder's exact match offsets).
//  The jump anchors (story UID + text range) are collected now but only consumed in Task 3.
//
//========================================================================================

#ifndef __KBSResultModel_h__
#define __KBSResultModel_h__

#include "IDFile.h"
#include "PMString.h"
#include "UIDRef.h"

#include <vector>

namespace KBSResultModel
{
	/** The panel shows at most this many hit rows (book order). The model still HOLDS every hit -
	    a same-book re-search reuses them, and a future export / replace consumes them ALL - only
	    the tree display is capped, to keep a huge result set from flooding the panel. */
	const int32 kKBSDisplayHitLimit = 500;

	/** One match on one line of one chapter. The three text segments are the line split around
	    the match; the jump anchors (Task 3) point back at the exact occurrence. */
	struct Hit
	{
		PMString	locator;	// the page locator "P<page>(<n>)" / "ov" (drawn at full text colour,
								// followed by a tab stop, ahead of the line)
		PMString	preText;	// the line's text before the match
		PMString	matchText;	// the matched text (drawn at full text colour)
		PMString	postText;	// the line's text after the match

		PMString	pageString;	// the match's page, Pages-panel style ("" = overset, no page)
		int32		pageIndex;	// the page's document order (-1 = overset); sorts hits into page order

		UID			storyUID;	// the story the match lives in (within its chapter's database)
		TextIndex	textStart;	// the match's start position in that story
		TextIndex	textEnd;	// the match's end position (Task 3 marker rectangle)

		Hit() : pageIndex(-1), storyUID(kInvalidUID), textStart(kInvalidTextIndex), textEnd(kInvalidTextIndex) {}
	};

	/** One chapter that holds at least one hit. */
	struct Chapter
	{
		PMString			name;	// the chapter's display name (its file name)
		UIDRef				docRef;	// current binding (Task 3 jump / reopen)
		IDFile				file;	// the chapter's .indd (Task 3 reopen of a closed chapter)
		std::vector<Hit>	hits;
	};

	/** Replace the whole model with a fresh search's chapters (only chapters with >=1 hit). */
	void SetResults(const std::vector<Chapter>& chapters);

	/** Forget the results (an empty search, or a teardown that still wants the tree emptied). */
	void Clear();

	/** Application-shutdown cleanup: release the vectors' storage, no UI. */
	void ShutdownCleanup();

	/** The number of chapters that hold a hit (uncapped; every chapter with >=1 hit). */
	int32 GetChapterCount();

	/** The number of hits under chapter 'chapterIdx' (uncapped, the full stored count). */
	int32 GetHitCount(int32 chapterIdx);

	/** The total number of hits across ALL chapters (uncapped) - for the status summary and a
	    future export. */
	int32 GetTotalHitCount();

	/** The number of chapters that have at least one DISPLAYED hit (the tree root's child count
	    under the display cap). Chapters past the cap are not shown. */
	int32 GetDisplayChapterCount();

	/** The number of hits DISPLAYED under chapter 'chapterIdx' - capped in book order so the whole
	    tree shows at most kKBSDisplayHitLimit hit rows. The chapter still STORES every hit. */
	int32 GetDisplayHitCount(int32 chapterIdx);

	/** A chapter node's display: its name and its hit count. false = index out of range. */
	bool GetChapterDisplay(int32 chapterIdx, PMString& outName, int32& outHitCount);

	/** A hit node's display: the page locator and the three line segments to paint. false = index
	    out of range. */
	bool GetHitDisplay(int32 chapterIdx, int32 hitIdx,
		PMString& outLocator, PMString& outPre, PMString& outMatch, PMString& outPost);

	/** A hit's jump anchors (Task 3): the chapter's document / file and the match's story +
	    text range. false = index out of range. */
	bool GetHitLocation(int32 chapterIdx, int32 hitIdx,
		UIDRef& outDocRef, IDFile& outFile, UID& outStoryUID, TextIndex& outStart, TextIndex& outEnd);

	/** Rebind a chapter's document reference (Task 3): after a closed chapter is reopened at jump
	    time, later jumps must use the live database, not the dead one from search time. */
	void RebindChapterDoc(int32 chapterIdx, const UIDRef& newDocRef);
}

#endif // __KBSResultModel_h__
