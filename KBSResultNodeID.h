//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  NodeID class for the result tree. The KBS tree is two levels (chapter -> hit), so a node is
//  the pair (chapter index, hit index):
//
//    (-1, -1)        the hidden root
//    (chap, -1)      a chapter row  -> index into KBSResultModel's chapters
//    (chap, hit)     a hit row      -> hit indexes that chapter's hits
//
//  The node's data (chapter name / count, hit text segments) is looked up from KBSResultModel by
//  these indices when needed, so nodes stay tiny and a rebuild after a new search is just
//  ClearTree + ChangeRoot (+ re-expanding). Ported from KESCL's KESCLResultNodeID, dropping the
//  third (doc/hit) level - itself modelled on the paneltreeview sample's PnlTrvFileNodeID.
//
//========================================================================================

#ifndef __KBSResultNodeID_h__
#define __KBSResultNodeID_h__

#include "NodeID.h"
#include "IPMStream.h"
#include "PMString.h"
#include "KBSID.h"

/** One node of the result tree: (chapter index, hit index); (-1, -1) is the hidden root,
    hit -1 marks a chapter row. */
class KBSResultNodeID : public NodeIDClass
{
public:
	enum { kNodeType = kKBSResultListWidgetBoss };

	/** The generic node the tree-view framework asks for (GetGenericNodeID). */
	static NodeID_rv Create() { return new KBSResultNodeID(); }

	/** A chapter row ('chapter' = 0-based chapter index, -1 = root). */
	static NodeID_rv Create(int32 chapter) { return new KBSResultNodeID(chapter, -1); }

	/** A hit row under chapter 'chapter'. */
	static NodeID_rv Create(int32 chapter, int32 hit) { return new KBSResultNodeID(chapter, hit); }

	virtual ~KBSResultNodeID() {}

	virtual NodeType GetNodeType() const { return kNodeType; }

	virtual int32 Compare(const NodeIDClass* nodeID) const
	{
		const KBSResultNodeID* other = static_cast<const KBSResultNodeID*>(nodeID);
		ASSERT(other);
		if (fChapter < other->fChapter)	return -1;
		if (fChapter > other->fChapter)	return 1;
		if (fHit < other->fHit)	return -1;
		if (fHit > other->fHit)	return 1;
		return 0;
	}

	virtual NodeIDClass* Clone() const { return new KBSResultNodeID(fChapter, fHit); }

	virtual void Read(IPMStream* stream)
	{
		stream->XferInt32(fChapter);
		stream->XferInt32(fHit);
	}

	virtual void Write(IPMStream* stream) const
	{
		stream->XferInt32(const_cast<KBSResultNodeID*>(this)->fChapter);
		stream->XferInt32(const_cast<KBSResultNodeID*>(this)->fHit);
	}

	/** The chapter's 0-based index into KBSResultModel (-1 = root). */
	int32 GetChapter() const { return fChapter; }

	/** The hit index within that chapter (-1 = this IS the chapter row, not a hit). */
	int32 GetHit() const { return fHit; }

	/** Is this a hit row (a leaf)? */
	bool16 IsHitRow() const { return fHit >= 0; }

	/** Debug aid, like the samples: makes tree-view asserts name the node. */
	virtual PMString GetDescription() const
	{
		PMString s("KBSResultRow ");
		s.AppendNumber(fChapter);
		if (fHit >= 0)
		{
			s.Append(":");
			s.AppendNumber(fHit);
		}
		s.SetTranslatable(kFalse);
		return s;
	}

private:
	// Private constructors force the factory methods, PnlTrvFileNodeID-style.
	KBSResultNodeID() : fChapter(-1), fHit(-1) {}
	KBSResultNodeID(int32 chapter, int32 hit) : fChapter(chapter), fHit(hit) {}

	int32 fChapter;
	int32 fHit;
};

#endif // __KBSResultNodeID_h__
