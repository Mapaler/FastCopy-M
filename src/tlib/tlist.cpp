static char *tlist_id = 
	"@(#)Copyright (C) 1996-2009 H.Shirouzu		tlist.cpp	Ver0.97";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: List Class
	Create					: 1996-06-01(Sat)
	Update					: 2011-05-23(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

/*
	TList class
*/
TList::TList(void)
{
	Init();
}

void TList::Init(void)
{
	top.prior = top.next = &top;
	num = 0;
}

void TList::AddObj(TListObj * obj)
{
	obj->prior = top.prior;
	obj->next = &top;
	top.prior->next = obj;
	top.prior = obj;
	num++;
}

void TList::DelObj(TListObj * obj)
{
//	if (!obj->next || !obj->prior || obj->next != &top && obj->prior != &top) {
//		Debug("DelObj(%p) (%p/%p)\n", obj, obj->next, obj->prior);
//	}
	if (obj->next) {
		obj->next->prior = obj->prior;
	}
	if (obj->prior) {
		obj->prior->next = obj->next;
	}
	obj->next = obj->prior = NULL;
	num--;
}

TListObj* TList::TopObj(void)
{
//	if (top.next != &top && top.next->next != &top && top.next->prior != &top) {
//		Debug("TopObj(%p) \n", top.next);
//	}
	return	top.next == &top ? NULL : top.next;
}

TListObj* TList::EndObj(void)
{
	return	top.next == &top ? NULL : top.prior;
}

TListObj* TList::NextObj(TListObj *obj)
{
	return	obj->next == &top ? NULL : obj->next;
}

TListObj* TList::PriorObj(TListObj *obj)
{
	return	obj->prior == &top ? NULL : obj->prior;
}

void TList::MoveList(TList *from_list)
{
	if (from_list->top.next != &from_list->top) {	// from_list is not empty
		if (top.next == &top) {	// empty
			top = from_list->top;
			top.next->prior = top.prior->next = &top;
		}
		else {
			top.prior->next = from_list->top.next;
			from_list->top.next->prior = top.prior;
			from_list->top.prior->next = &top;
			top.prior = from_list->top.prior;
		}
		num += from_list->num;
		from_list->Init();
	}
}

/*
	TRecycleList class
*/
TRecycleList::TRecycleList(int init_cnt, int size)
{
	data = new char [init_cnt * size];
	memset(data, 0, init_cnt * size);

	for (int cnt=0; cnt < init_cnt; cnt++) {
		TListObj *obj = (TListObj *)(data + cnt * size);
		list[FREE_LIST].AddObj(obj);
	}
}

TRecycleList::~TRecycleList()
{
	delete [] data;
}

TListObj *TRecycleList::GetObj(int list_type)
{
	TListObj	*obj = list[list_type].TopObj();

	if (obj)
		list[list_type].DelObj(obj);

	return	obj;
}

void TRecycleList::PutObj(int list_type, TListObj *obj)
{
	list[list_type].AddObj(obj);
}

