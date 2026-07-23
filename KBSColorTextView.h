//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  IKBSRowData: the per-row draw data for a hit line's colour cell. The line is pre-split (in
//  KBSSearchEngine, against the paragraph's wide string at the finder's exact offsets) into
//  three segments - the text before the match, the matched text, and the text after - so the
//  cell (KBSColorTextView) just paints three runs and never does UTF-16 boundary maths at draw
//  time. The matched run is drawn in a highlight colour; the rest follows the palette text
//  colour (theme-adaptive). New to KBS (no KESCL original); the recipe is the multi-colour cell
//  draw pattern proven against customdatalinkui's DVControlView.
//
//========================================================================================

#ifndef __KBSColorTextView_h__
#define __KBSColorTextView_h__

#include "IPMUnknown.h"
#include "PMString.h"
#include "KBSID.h"

/** The three text segments a hit row's colour cell paints: the text before the match, the
    matched text (highlighted), and the text after. Set by the widget manager on every apply,
    read by KBSColorTextView::Draw. Non-persistent (rebuilt on every search). */
class IKBSRowData : public IPMUnknown
{
public:
	enum { kDefaultIID = IID_IKBSROWDATA };

	/** Replace this row's three text segments (any may be empty). */
	virtual void SetSegments(const PMString& pre, const PMString& match, const PMString& post) = 0;

	/** Read this row's three text segments back (for the cell's Draw). */
	virtual void GetSegments(PMString& outPre, PMString& outMatch, PMString& outPost) const = 0;
};

#endif // __KBSColorTextView_h__
