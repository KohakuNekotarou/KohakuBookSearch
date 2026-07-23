//========================================================================================
//  
//  $File: $
//  
//  Owner: 
//  
//  $Author: $
//  
//  $DateTime: $
//  
//  $Revision: $
//  
//  $Change: $
//  
//  Copyright 1997-2012 Adobe Systems Incorporated. All rights reserved.
//  
//  NOTICE:  Adobe permits you to use, modify, and distribute this file in accordance 
//  with the terms of the Adobe license agreement accompanying it.  If you have received
//  this file from a source other than Adobe, then your use, modification, or 
//  distribution of it requires the prior written permission of Adobe.
//  
//========================================================================================

#include "VCPlugInHeaders.h"

// Interface includes:

// Interface includes:
#include "IPalettePanelUtils.h"	// QueryPanelByWidgetID - reach the panel's status text
#include "IPanelControlData.h"	// FindWidget
#include "IControlView.h"		// Invalidate / ForceRedraw
#include "ITextControlData.h"	// SetString

// General includes:
#include "CActionComponent.h"
#include "CAlert.h"
#include "IActionStateList.h"	// UpdateActionStates: check mark for the Hide Previous Chapter toggle
#include "Utils.h"

// Project includes:
#include "KBSID.h"
#include "KBSSearchEngine.h"
#include "KBSResultTree.h"		// rebuild the result tree after a search

/** Implements IActionComponent; performs the actions that are executed when the plug-in's
	menu items are selected.

	
	@ingroup kohakubooksearch

*/
class KBSActionComponent : public CActionComponent
{
public:
/**
 Constructor.
 @param boss interface ptr from boss object on which this interface is aggregated.
 */
		KBSActionComponent(IPMUnknown* boss);

		/** The action component should perform the requested action.
			This is where the menu item's action is taken.
			When a menu item is selected, the Menu Manager determines
			which plug-in is responsible for it, and calls its DoAction
			with the ID for the menu item chosen.

			@param actionID identifies the menu item that was selected.
			@param ac active context
			@param mousePoint contains the global mouse location at time of event causing action (e.g. context menus). kInvalidMousePoint if not relevant.
			@param widget contains the widget that invoked this action. May be nil. 
			*/
		virtual void DoAction(IActiveContext* ac, ActionID actionID, GSysPoint mousePoint, IPMUnknown* widget);

			/** Custom-enabled actions (the Hide Previous Chapter toggle) get their check mark here. */
			virtual void UpdateActionStates(IActiveContext* ac, IActionStateList* listToUpdate, GSysPoint mousePoint, IPMUnknown* widget);

	private:
		/** Encapsulates functionality for the about menu item. */
		void DoAbout();
		


};

/* CREATE_PMINTERFACE
 Binds the C++ implementation class onto its
 ImplementationID making the C++ code callable by the
 application.
*/
CREATE_PMINTERFACE(KBSActionComponent, kKBSActionComponentImpl)

// Session-only toggle state (not persisted). Real "close previous chapter on jump"
// behaviour is connected in a later step; for now this only drives the check mark.
static bool16 sHidePrevChapter = kFalse;

/* ShowStatus
	Write a one-line message to the panel's status read-out. A single-line StaticText does not
	repaint on SetString alone, so invalidate + force a redraw (see the SDK note on immediate
	StaticText updates). No-op when the panel is not open.
*/
static void ShowStatus(const PMString& message)
{
	InterfacePtr<IPanelControlData> panelData(Utils<IPalettePanelUtils>()->QueryPanelByWidgetID(kKBSPanelWidgetID));
	if (panelData == nil)
		return;
	IControlView* textView = panelData->FindWidget(kKBSStaticTextWidgetID);
	if (textView == nil)
		return;
	InterfacePtr<ITextControlData> textData(textView, UseDefaultIID());
	if (textData == nil)
		return;
	textData->SetString(message, kTrue /*invalidate*/, kFalse /*don't notify*/);
	textView->Invalidate();
	textView->ForceRedraw();
}

/* KBSActionComponent Constructor
*/
KBSActionComponent::KBSActionComponent(IPMUnknown* boss)
: CActionComponent(boss)
{
}

/* DoAction
*/
void KBSActionComponent::DoAction(IActiveContext* ac, ActionID actionID, GSysPoint mousePoint, IPMUnknown* widget)
{
	switch (actionID.Get())
	{

		case kKBSPopupAboutThisActionID:
		case kKBSAboutActionID:
		{
			this->DoAbout();
			break;
		}

		case kKBSSearchBookActionID:
		{
			// Search the active book (or the front document) with the user's current
			// Find/Change query. The engine fills KBSResultModel with the hits (grouped by
			// chapter); rebuild the result tree from it, and show the summary on the status line.
			PMString summary;
			KBSSearchEngine::SearchBook(summary);
			KBSResultTree::Rebuild();
			ShowStatus(summary);
			break;
		}

		case kKBSHidePrevChapterActionID:
		{
			// Toggle the session-only flag. Its check mark is drawn in UpdateActionStates.
			sHidePrevChapter = !sHidePrevChapter;
			break;
		}



		default:
		{
			break;
		}
	}
}

/* DoAbout
*/
void KBSActionComponent::DoAbout()
{
	CAlert::ModalAlert
	(
		kKBSAboutBoxStringKey,				// Alert string
		kOKString, 						// OK button
		kNullString, 						// No second button
		kNullString, 						// No third button
		1,							// Set OK button to default
		CAlert::eInformationIcon				// Information icon.
	);
}

/* UpdateActionStates
*/
void KBSActionComponent::UpdateActionStates(IActiveContext* /*ac*/, IActionStateList* listToUpdate, GSysPoint /*mousePoint*/, IPMUnknown* /*widget*/)
{
	for (int32 i = 0; i < listToUpdate->Length(); i++)
	{
		const ActionID action = listToUpdate->GetNthAction(i);

		if (action == kKBSHidePrevChapterActionID)
		{
			int16 actionState = kEnabledAction;
			if (sHidePrevChapter)
				actionState |= kSelectedAction;		// show the check mark when ON
			listToUpdate->SetNthActionState(i, actionState);
		}
	}
}


