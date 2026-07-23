//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  Result tree rebuild entry point. Called after KBSResultModel has been filled by a search:
//  reloads the panel's tree widget from the model and expands every chapter so the hit rows are
//  visible (expanding also PRIMES each chapter's expander arrow so it is actually drawn - the
//  tree framework draws no arrow for a node whose children were never materialized). No-op when
//  the panel is closed. Implemented in KBSResultListWidgetMgr.cpp (it lives with the tree).
//
//========================================================================================

#ifndef __KBSResultTree_h__
#define __KBSResultTree_h__

namespace KBSResultTree
{
	/** (Re)load the panel's result tree from KBSResultModel and expand every chapter. Safe to
	    call when the panel is closed (does nothing then). */
	void Rebuild();
}

#endif // __KBSResultTree_h__
