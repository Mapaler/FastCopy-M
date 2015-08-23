static char *regexp_id = 
	"@(#)Copyright (C) 2005-2015 H.Shirouzu		regexp.cpp	ver3.00";
/* ========================================================================
	Project  Name			: Regular Expression / Wild Card Match Library
	Create					: 2005-11-03(The)
	Update					: 2015-06-22(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#include "tlib/tlib.h"
#include "regexp.h"

RegExp::RegExp()
{
	memset(states_tbl, 0, sizeof(states_tbl));
	epsilon_num = 0;
	epsilon_tbl = NULL;
	max_state = 0;
}

RegExp::~RegExp()
{
	Init();
}

void RegExp::Init()
{
	for (int type=NORMAL_TBL; type < MAX_STATES_TBL; type++) {
		if (states_tbl[type]) {
			for (int i=0; i < BYTE_NUM; i++) {
				if (states_tbl[type][i]) {
					for (int j=0; j < BYTE_NUM; j++) {
						if (states_tbl[type][i][j]) {
							delete states_tbl[type][i][j];
						}
					}
					delete [] states_tbl[type][i];
				}
			}
			delete [] states_tbl[type];
			states_tbl[type] = NULL;
		}
	}

	if (epsilon_tbl) {
		for (int i=0; i < epsilon_num; i++) {
			if (epsilon_tbl[i]) delete epsilon_tbl[i];
		}
		free(epsilon_tbl);
		epsilon_tbl = NULL;
	}
	max_state = 0;
	epsilon_num = 0;
	end_states.Reset();
}

void RegExp::AddRegState(StatesType type, WCHAR ch, int state)
{
	int	idx = ch >> BITS_OF_BYTE;

	RegStates	***&tbl = states_tbl[type];

	if (!tbl) {
		tbl = new RegStates **[BYTE_NUM];
		memset(tbl, 0, sizeof(RegStates **) * BYTE_NUM);
	}

	RegStates	**&sub_tbl = tbl[idx];

	if (!sub_tbl) {
		sub_tbl = new RegStates *[BYTE_NUM];
		memset(sub_tbl, 0, sizeof(RegStates *) * BYTE_NUM);
	}

	ch = (u_char)ch;
	if (!sub_tbl[ch]) {
		sub_tbl[ch] = new RegStates(max_state);
	}
	sub_tbl[ch]->AddState(state);
}

RegStates *RegExp::GetRegStates(StatesType type, WCHAR ch)
{
	RegStates	***&tbl = states_tbl[type];
	RegStates	**sub_tbl = tbl ? tbl[ch >> BITS_OF_BYTE] : NULL;

	return	sub_tbl ? sub_tbl[(u_char)ch] : NULL;
}

void RegExp::AddEpState(int state, int new_state)
{
	RegStates	*&tbl = epsilon_tbl[state];

	if (!tbl) tbl = new RegStates(max_state);

	tbl->AddState(new_state);
}

void RegExp::GetEpStates(const RegStates& cur_states, RegStates *ret_states)
{
	ret_states->Reset();

	for (int idx=0; cur_states.GetBitIdx(idx, &idx); idx++) {
		if (epsilon_tbl[idx]) {
			*ret_states |= *(epsilon_tbl[idx]);
		}
	}
}

void RegExp::AddRegStateEx(StatesType type, WCHAR ch, int state, RegExp::CaseSense cs)
{
	if (cs == CASE_SENSE)
		AddRegState(type, ch, state);
	else {
		AddRegState(type, (WCHAR)::CharLowerW((WCHAR *)ch), state);
		AddRegState(type, (WCHAR)::CharUpperW((WCHAR *)ch), state);

		// CASE_INSENSE_SLASH mode では '/' 入力で '\\' も受理するように
		if (cs == CASE_INSENSE_SLASH && (ch == '/' || ch == '\\')) {
			AddRegState(type, (ch == '/') ? '\\' :  '/', state);
		}
	}
}

bool RegExp::SetupEpTable(int append_state)
{
#define BIG_ALLOC 128
	int	state_limits = append_state + max_state + 2;
	int	new_epnum    = ALIGN_SIZE(state_limits, BIG_ALLOC);

	if (epsilon_num >= new_epnum) return true;
	if (new_epnum >= MAX_REGST_STATE) return false;

	epsilon_tbl = (RegStates **)realloc(epsilon_tbl, sizeof(RegStates *) * new_epnum);

	if (!epsilon_tbl) return false;

	for (int i=epsilon_num; i < new_epnum; i++) {
		epsilon_tbl[i] = NULL;
	}
	epsilon_num = new_epnum;

	return	true;
}


bool RegExp::RegisterWildCard(const WCHAR *wild_str, RegExp::CaseSense cs)
{
	if (!SetupEpTable((int)wcslen(wild_str))) return false;

	max_state++;
	AddEpState(0, max_state);

	enum Mode { NORMAL, CHARCLASS } mode = NORMAL;
	enum SubMode { DEFAULT, CC_START, CC_NORMAL, CC_RANGE, CC_END } submode = DEFAULT;

	int			escape = 0;
	StatesType	type = NORMAL_TBL;
	WCHAR		ch = 0, last_ch, start_ch, end_ch;

	do {
		last_ch = ch;
		ch = *wild_str++;	// 0 も文末判定文字として登録

		if (mode == NORMAL) {
			switch (ch) {
			case '[':
				mode = CHARCLASS;
				submode = CC_START;
				continue;

			case '?':
				AddEpState(max_state, max_state + 1);
				max_state++;
				AddRegStateEx(REV_TBL, '\0', max_state, cs);
				if (cs == CASE_INSENSE_SLASH) {
					AddRegStateEx(REV_TBL, '/', max_state, cs);
				}
				continue;

			case '*':
				AddEpState(max_state, max_state);
				if (cs == CASE_INSENSE_SLASH)
					AddRegStateEx(REV_TBL, '/', max_state, cs);
				continue;
			}
			if (ch || last_ch != '*') {	// '*' で終了した場合は '\0'まで確認せずに判定終了
				max_state++;
				AddRegStateEx(type, ch, max_state, cs);
			}
		}
		else {	// CHARCLASS mode
			switch (escape) {
			case 1:  escape = 2; break;
			case 2:  escape = 0; break;
			default: if (ch == '\\') { escape = 1; continue; }
			}
			if (submode == CC_START) {
				submode = CC_NORMAL;
				end_ch = 0;
				if ((ch == '^' || ch == '!') && !escape) {
					type = REV_TBL;
					continue;
				}
			}
			else if (submode == CC_RANGE) {
				while (start_ch <= ch) {
					AddRegStateEx(type, start_ch, max_state + 1, cs);
					start_ch++;
				}
				if (type == REV_TBL)
					AddEpState(max_state, max_state + 1);
				submode = CC_NORMAL;
				continue;
			}

			if (ch == ']' && !escape) {
				if (submode == CC_RANGE) return false;
				mode = NORMAL;
				type = NORMAL_TBL;
				submode = CC_END;
				max_state++;
				continue;
			}
			if (ch == '-' && !escape) {
				submode = CC_RANGE;
				start_ch = end_ch;
				continue;
			}
			AddRegStateEx(type, end_ch = ch, max_state + 1, cs);
			if (type == REV_TBL) {
				AddEpState(max_state, max_state + 1);
			}
			AddRegStateEx(REV_TBL, '\0', max_state, cs);
		}
	} while (ch);

	if (mode == CHARCLASS) return false;

	end_states.AddState(max_state);

	return	true;
}

bool RegExp::IsMatch(const WCHAR *target)
{
	if (!epsilon_tbl) return false;

	RegStates	total_states(max_state);
	RegStates	ep_states(max_state);
	RegStates	tmp_states(max_state);

	tmp_states.AddState(0);

	GetEpStates(tmp_states, &total_states);

	while (!total_states.IsZero()) {
		WCHAR		ch		= *target++;
		RegStates	*normal	= GetRegStates(NORMAL_TBL, ch);
		RegStates	*rev	= GetRegStates(REV_TBL,    ch);

		GetEpStates(total_states, &ep_states);
		total_states.ShiftLeft();

		if (normal) {
			total_states &= *normal;
		} else {
			total_states.Reset();
		}

		if (rev) {
			rev->GetReverse(&tmp_states);
			ep_states &= tmp_states;
		}

		total_states |= ep_states;

		if (total_states.HasCommonBits(end_states)) return true;
		if (!ch) break;
	}

	return	false;
}

