//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  ITreeViewWidgetMgr for the result tree. Two node kinds:
//
//    * CHAPTER rows (from kKBSResultChapterNodeWidgetRsrcID): the chapter's name and its hit
//      count, "<name>  (N)", in a stock InfoStaticText after the expander arrow's zone. The
//      expander is hidden on a chapter with no hits (never happens - only chapters with hits are
//      in the model - but the guard mirrors KESCL's).
//    * HIT rows (from kKBSResultHitNodeWidgetRsrcID): one match's line, drawn by the custom
//      colour cell (KBSColorTextView) with the matched part highlighted. No expander (a leaf);
//      indented one more zone than the chapter row.
//
//  The visual indent is drawn by explicit frame offsets in ApplyNodeIDToWidget (the framework
//  indent is unused, as in KESCL). This file also hosts KBSResultTree::Rebuild (the tree lives
//  here). Ported from KESCL's KESCLResultListWidgetMgr, simplified to two levels - itself
//  modelled on the paneltreeview sample's PnlTrvTVWidgetMgr.
//
//========================================================================================

#include "VCPlugInHeaders.h"

// Interface includes:
#include "IControlView.h"
#include "IPalettePanelUtils.h"			// QueryPanelByWidgetID (the rebuild reaches the tree)
#include "IPanelControlData.h"
#include "ITextControlData.h"
#include "ITreeViewHierarchyAdapter.h"	// child count - decides the expander's visibility
#include "ITreeViewMgr.h"				// ClearTree / ChangeRoot / ExpandNode (the rebuild)

// General includes:
#include "CTreeViewWidgetMgr.h"
#include "CreateObject.h"
#include "CoreResTypes.h"
#include "LocaleSetting.h"
#include "PMString.h"
#include "RsrcSpec.h"
#include "Utils.h"
#include "widgetid.h"		// kTreeNodeExpanderWidgetID

// Project includes:
#include "KBSID.h"
#include "KBSResultNodeID.h"
#include "KBSResultModel.h"
#include "KBSResultTree.h"
#include "KBSColorTextView.h"	// IKBSRowData (the hit cell)

namespace
{
	// Visual layout of a row (px inside the 19px row). The chapter row's cells start after the
	// expander's zone; the hit row's cell steps one more zone right (drawn by us - the framework
	// indent is off, as in KESCL).
	const PMReal kRowInset = 2.0;
	const PMReal kExpanderZone = 16.0;

	// Put 'text' into a row's static-text cell. Row widgets are recycled as the tree scrolls, so
	// every cell is written on every apply. No manual repaint: the tree draws the row right after.
	void SetColumnText(const InterfacePtr<IPanelControlData>& rowData, const WidgetID& wid, const PMString& text)
	{
		if (rowData == nil)
			return;
		IControlView* cv = rowData->FindWidget(wid);
		if (cv == nil)
			return;
		InterfacePtr<ITextControlData> tcd(cv, UseDefaultIID());
		if (tcd != nil)
			tcd->SetString(text, kTrue /*invalidate*/, kFalse /*don't notify*/);
	}
}

/** Builds and fills the result tree's row widgets (chapter rows and hit rows). */
class KBSResultListWidgetMgr : public CTreeViewWidgetMgr
{
public:
	// kHierarchical: the framework tracks expansion and asks the adapter for the child level.
	KBSResultListWidgetMgr(IPMUnknown* boss) : CTreeViewWidgetMgr(boss, kHierarchical) {}
	virtual ~KBSResultListWidgetMgr() {}

	virtual IControlView* CreateWidgetForNode(const NodeID& node) const
	{
		TreeNodePtr<KBSResultNodeID> nodeID(node);
		RsrcID rsrcID = kKBSResultChapterNodeWidgetRsrcID;
		if (nodeID != nil && nodeID->IsHitRow())
			rsrcID = kKBSResultHitNodeWidgetRsrcID;
		IControlView* retval = (IControlView*)::CreateObject(
			::GetDataBase(this),
			RsrcSpec(LocaleSetting::GetLocale(), kKBSPluginID, kViewRsrcType, rsrcID),
			IID_ICONTROLVIEW);
		ASSERT(retval);
		return retval;
	}

	virtual WidgetID GetWidgetTypeForNode(const NodeID& node) const
	{
		TreeNodePtr<KBSResultNodeID> nodeID(node);
		if (nodeID != nil && nodeID->IsHitRow())
			return kKBSResultHitNodeWidgetID;
		return kKBSResultChapterNodeWidgetID;
	}

	virtual bool16 ApplyNodeIDToWidget(const NodeID& node, IControlView* widget, int32 message = 0) const
	{
		CTreeViewWidgetMgr::ApplyNodeIDToWidget(node, widget);

		TreeNodePtr<KBSResultNodeID> nodeID(node);
		if (nodeID == nil)
			return kTrue;

		InterfacePtr<IPanelControlData> rowData(widget, UseDefaultIID());
		if (rowData == nil)
			return kTrue;

		if (nodeID->IsHitRow())
			this->ApplyHitRow(nodeID, widget, rowData);
		else
			this->ApplyChapterRow(nodeID, node, widget, rowData);
		return kTrue;
	}

	virtual PMReal GetIndentForNode(const NodeID& node) const
	{
		// PER-LEVEL indent (the base sums these up the ancestor chain). Unused in practice (the
		// framework indent is off, as in KESCL); the Apply*Row methods draw the hierarchy with
		// explicit offsets. Kept consistent in case a framework path ever consults it.
		TreeNodePtr<KBSResultNodeID> nodeID(node);
		if (nodeID != nil && nodeID->IsHitRow())
			return PMReal(kExpanderZone);
		return 0.0;
	}

private:
	// A chapter row: its expander (shown when it has hits) and "<name>  (N)" after the zone.
	void ApplyChapterRow(const TreeNodePtr<KBSResultNodeID>& nodeID, const NodeID& node,
		IControlView* widget, const InterfacePtr<IPanelControlData>& rowData) const
	{
		PMString name;
		int32 hitCount = 0;
		if (!KBSResultModel::GetChapterDisplay(nodeID->GetChapter(), name, hitCount))
			return;

		// The expander arrow: visible exactly when the row has children. Hide-only would still
		// leave a click target (the stacked-widget lesson), so the hidden arrow is disabled too.
		IControlView* expander = rowData->FindWidget(kTreeNodeExpanderWidgetID);
		if (expander != nil)
		{
			InterfacePtr<const ITreeViewHierarchyAdapter> adapter(this, UseDefaultIID());
			const bool16 hasChildren =
				(adapter != nil && adapter->GetNumChildren(node) > 0) ? kTrue : kFalse;
			if (hasChildren)
			{
				expander->ShowView();
				expander->Enable();
			}
			else
			{
				expander->HideView();
				expander->Disable();
			}
		}

		// "<name>  (N)": the hit count in parentheses after the chapter name.
		PMString label(name);
		label.SetTranslatable(kFalse);
		label.Append("  (");
		label.AppendNumber(hitCount);
		label.Append(")");
		SetColumnText(rowData, kKBSResultChapterLabelWidgetID, label);
	}

	// A hit row: the match's line into the custom colour cell (three segments), no expander,
	// indented one zone deeper than the chapter row.
	void ApplyHitRow(const TreeNodePtr<KBSResultNodeID>& nodeID, IControlView* widget,
		const InterfacePtr<IPanelControlData>& rowData) const
	{
		PMString pre, match, post;
		if (!KBSResultModel::GetHitDisplay(nodeID->GetChapter(), nodeID->GetHit(), pre, match, post))
			return;

		// Draw our own indent: the hit cell steps one expander zone right of the chapter row's
		// text start, running to the row's right edge.
		const PMReal rowRight = widget->GetFrame().Width() - kRowInset;
		const PMReal xStart = kRowInset + 2 * kExpanderZone;

		IControlView* cell = rowData->FindWidget(kKBSResultTextWidgetID);
		if (cell != nil)
		{
			PMRect frame = cell->GetFrame();
			frame.Left(xStart);
			frame.Right(rowRight);
			cell->SetFrame(frame);

			// Hand the three text segments to the colour cell; it invalidates itself as the tree
			// draws the row right after.
			InterfacePtr<IKBSRowData> data(cell, UseDefaultIID());
			if (data != nil)
				data->SetSegments(pre, match, post);
			cell->Invalidate();
		}
	}
};

CREATE_PMINTERFACE(KBSResultListWidgetMgr, kKBSResultListWidgetMgrImpl)

//----------------------------------------------------------------------------------------
// KBSResultTree::Rebuild - reload the panel's tree from the model, expand every chapter
//----------------------------------------------------------------------------------------

void KBSResultTree::Rebuild()
{
	// Reach the tree widget through the panel; nil when the panel is closed (do nothing then).
	InterfacePtr<IPanelControlData> panelData(Utils<IPalettePanelUtils>()->QueryPanelByWidgetID(kKBSPanelWidgetID));
	if (panelData == nil)
		return;
	IControlView* listView = panelData->FindWidget(kKBSResultListWidgetID);
	if (listView == nil)
		return;
	InterfacePtr<ITreeViewMgr> treeMgr(listView, UseDefaultIID());
	if (treeMgr == nil)
		return;

	// ClearTree(kTrue) forgets the old expansion state (rebuilt by the priming below);
	// ChangeRoot(kTrue) says every row widget has the same height, which they do (chapter and
	// hit rows are both 19px).
	treeMgr->ClearTree(kTrue);
	treeMgr->ChangeRoot(kTrue);

	// Expand every chapter: this both shows the hit rows (the KBS default is fully expanded) AND
	// PRIMES each chapter's expander arrow - the framework draws no arrow for a node whose
	// children were never materialized, and expanding materializes them (the "no expander arrow"
	// fix, KESCL/paneltreeview). A chapter with no hits is not in the model, so every chapter
	// node here has children.
	const int32 chapters = KBSResultModel::GetChapterCount();
	for (int32 c = 0; c < chapters; ++c)
		treeMgr->ExpandNode(KBSResultNodeID::Create(c), kFalse);
}

// End, KBSResultListWidgetMgr.cpp.
