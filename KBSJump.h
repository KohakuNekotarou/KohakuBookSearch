//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  Jump-to-hit navigation (Task 3). A hit-row click asks JumpToHit(chapter, hit) to: resolve the
//  hit's stored location (reopening the chapter windowless if the user closed it), bring that
//  chapter's window to the front, scroll the view so the match is centred, and raise the red
//  marker (KBSDrawEventHandler) which fades itself out. It does NOT select the text - it points at
//  the match, VS-style. With "Hide Previous Chapter" ON, every other displayed clean document is
//  closed as the jump lands. Ported from KESCL's jump machinery (KESCL left untouched), simplified
//  to a static snapshot (no match-list navigation, no edit-repair, no reverse mode).
//
//========================================================================================

#ifndef __KBSJump_h__
#define __KBSJump_h__

namespace KBSJump
{
	/** Jump to hit 'hitIdx' of chapter 'chapterIdx' in KBSResultModel: front its document, scroll
	    to the match, raise the marker. Does not select. No-op on a bad index or an unreachable
	    chapter (missing / locked file). An overset match has no on-page location, so the view is
	    left as-is and no marker is shown (the overset "+" locator is Task 4). */
	void JumpToHit(int32 chapterIdx, int32 hitIdx);

	/** The "Hide Previous Chapter" flyout toggle (session state; starts ON). Read by JumpToHit to
	    decide whether to close other displayed chapters as a jump lands; the flyout drives it. */
	bool IsHidePreviousChapterOn();
	void ToggleHidePreviousChapter();
}

#endif // __KBSJump_h__
