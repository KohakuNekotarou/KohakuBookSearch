//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  KBSColorTextView: a self-drawing tree cell that paints a hit line with the matched part in a
//  highlight colour (VS "Find in Files" style). A stock StaticText draws one colour per row, so
//  the hit rows use this DVControlView-derived cell instead, drawing three runs left to right
//  (before / matched / after). KBSRowData is the tiny data holder aggregated on the same boss.
//
//  Recipe: the multi-colour cell draw proven against customdatalinkui's DVControlView -
//  AGMGraphicsContext + StringUtils::PMDrawString / PMDrawStringRGB, the palette font from
//  IInterfaceFonts, the baseline from IWidgetUtils::GetViewYPosition. convertAmpersand is kFalse
//  on BOTH the draw and the measure so a literal '&' in the search text is neither underlined
//  nor dropped. Long lines clip at the cell's right edge for now (ellipsis is Task 4 polish).
//
//========================================================================================

#include "VCPlugInHeaders.h"

// Interface includes:
#include "IGraphicsPort.h"
#include "IInterfaceColors.h"	// RealAGMColor, InterfaceColor indices
#include "IInterfaceFonts.h"	// the palette window font

// General includes:
#include "AGMGraphicsContext.h"
#include "AutoGSave.h"
#include "CPMUnknown.h"
#include "DVControlView.h"
#include "DrawStringUtils.h"	// StringUtils::PMDrawString / PMDrawStringRGB / PMMeasureString
#include "ISession.h"			// GetExecutionContextSession
#include "IWidgetUtils.h"		// GetViewYPosition
#include "ShuksanID.h"			// kPaletteWindowFontId
#include "Utils.h"

// Project includes:
#include "KBSID.h"
#include "KBSColorTextView.h"

// Linear blend of two RGB colours (t = 0 -> bg, t = 1 -> fg). Used to fade the context text
// toward the panel background (the KESCM scrollbar-map trick).
static RealAGMColor BlendColor(const RealAGMColor& bg, const RealAGMColor& fg, const PMReal& t)
{
	const PMReal u = PMReal(1.0) - t;
	return RealAGMColor(
		ToDouble(bg.red   * u + fg.red   * t),
		ToDouble(bg.green * u + fg.green * t),
		ToDouble(bg.blue  * u + fg.blue  * t));
}

//----------------------------------------------------------------------------------------
// KBSRowData - the per-row data holder (three text segments)
//----------------------------------------------------------------------------------------

/** Non-persistent holder for a hit row's three text segments, aggregated on the colour cell's
    boss beside the view. Written by the widget manager on every apply. */
class KBSRowData : public CPMUnknown<IKBSRowData>
{
public:
	KBSRowData(IPMUnknown* boss) : CPMUnknown<IKBSRowData>(boss) {}
	virtual ~KBSRowData() {}

	virtual void SetSegments(const PMString& pre, const PMString& match, const PMString& post)
	{
		fPre = pre;   fPre.SetTranslatable(kFalse);
		fMatch = match; fMatch.SetTranslatable(kFalse);
		fPost = post; fPost.SetTranslatable(kFalse);
	}

	virtual void GetSegments(PMString& outPre, PMString& outMatch, PMString& outPost) const
	{
		outPre = fPre;
		outMatch = fMatch;
		outPost = fPost;
	}

private:
	PMString fPre;
	PMString fMatch;
	PMString fPost;
};

CREATE_PMINTERFACE(KBSRowData, kKBSRowDataImpl)

//----------------------------------------------------------------------------------------
// KBSColorTextView - the self-drawing cell
//----------------------------------------------------------------------------------------

/** Implements IControlView: draws a hit line with the matched part highlighted. */
class KBSColorTextView : public DVControlView
{
	typedef DVControlView inherited;
public:
	KBSColorTextView(IPMUnknown* boss) : inherited(boss) {}
	virtual ~KBSColorTextView() {}

	virtual void Draw(IViewPort* viewPort, SysRgn updateRgn);
};

CREATE_PERSIST_PMINTERFACE(KBSColorTextView, kKBSColorTextViewImpl)

void KBSColorTextView::Draw(IViewPort* viewPort, SysRgn updateRgn)
{
	AGMGraphicsContext gc(viewPort, this, updateRgn);
	InterfacePtr<IGraphicsPort> gPort(gc.GetViewPort(), UseDefaultIID());
	if (gPort == nil)
		return;
	AutoGSave gSave(gPort);

	InterfacePtr<IKBSRowData> data(this, UseDefaultIID());
	if (data == nil)
		return;

	PMString pre, match, post;
	data->GetSegments(pre, match, post);

	// The palette window's font (same one KESCL's report panel measures with).
	InterfacePtr<IInterfaceFonts> fonts(GetExecutionContextSession(), UseDefaultIID());
	if (fonts == nil)
		return;
	const InterfaceFontInfo& fontInfo = fonts->GetFont(kPaletteWindowFontId);

	const PMRect frame = this->GetInnerContentFrame();
	const PMReal y = Utils<IWidgetUtils>()->GetViewYPosition(&gc, fontInfo, frame.Height());
	const PMReal rightEdge = frame.Right();
	PMReal x = frame.Left();

	// Colours, entirely from the current theme so KBS matches whatever colours it uses:
	//   * bg = the panel's background fill (kInterfacePaletteFill)
	//   * fg = the theme's TEXT colour (kInterfaceTextColor - exactly what InDesign's own panels
	//          draw text with: black in a light UI, ~0.8 gray in a dark one; it flips with the
	//          theme, so nothing is hardcoded and nothing vanishes when the UI brightness changes)
	// The matched text is drawn at the full theme text colour; the context (the "P<page>(<n>)"
	// locator and the rest of the line) is that same colour faded toward the background, so the
	// match reads at full strength and the context recedes. Tune kContextTextWeight to taste:
	// 0 = fully the background (invisible), 1 = the full text colour (no fade).
	const PMReal kContextTextWeight(0.50);
	RealAGMColor bg(0.5, 0.5, 0.5), fg(0.0, 0.0, 0.0);	// sane fallbacks if the query fails
	InterfacePtr<IInterfaceColors> colors(GetExecutionContextSession(), UseDefaultIID());
	if (colors != nil)
	{
		colors->GetRealAGMColor(kInterfacePaletteFill, bg);
		colors->GetRealAGMColor(kInterfaceTextColor, fg);
	}
	const RealAGMColor kMatchColor = fg;								// the theme's text colour
	const RealAGMColor kContextColor = BlendColor(bg, fg, kContextTextWeight);	// faded toward bg

	// Three runs, left to right. convertAmpersand=kFalse on draw AND measure. A run whose start
	// is already past the cell's right edge is skipped (the earlier runs consumed the width).
	if (!pre.IsEmpty() && x < rightEdge)
	{
		StringUtils::PMDrawStringRGB(&gc, PMPoint(x, y), pre, fontInfo, kContextColor, kFalse, kFalse);
		x += StringUtils::PMMeasureString(&gc, pre, fontInfo, kFalse).X();
	}
	if (!match.IsEmpty() && x < rightEdge)
	{
		StringUtils::PMDrawStringRGB(&gc, PMPoint(x, y), match, fontInfo, kMatchColor, kFalse, kFalse);
		x += StringUtils::PMMeasureString(&gc, match, fontInfo, kFalse).X();
	}
	if (!post.IsEmpty() && x < rightEdge)
	{
		StringUtils::PMDrawStringRGB(&gc, PMPoint(x, y), post, fontInfo, kContextColor, kFalse, kFalse);
	}
}

// End, KBSColorTextView.cpp.
