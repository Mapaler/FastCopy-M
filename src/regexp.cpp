static char *regexp_id = 
	"@(#)Copyright (C) 2005-2006 H.Shirouzu		regexp.cpp	ver1.84";
/* ========================================================================
	Project  Name			: Regular Expression / Wild Card Match Library
	Create					: 2005-11-03(The)
	Update					: 2006-01-31(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib/tlib.h"
#include "regexp.h"


RegExp::RegExp()
{
	memset(states_tbl, 0, sizeof(states_tbl));
	epsilon_tbl = NULL;
	end_states = 0;
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
				if (states_tbl[type][i])
					delete states_tbl[type][i];
			}
			delete [] states_tbl[type];
			states_tbl[type] = NULL;
		}
	}
	if (epsilon_tbl) {
		delete [] epsilon_tbl;
		epsilon_tbl = NULL;
	}
	max_state = 0;
	end_states = 0;
}

void RegExp::AddRegStates(StatesType type, WCHAR ch, const RegStates &state_pattern)
{
	int	idx = ch >> BITS_OF_BYTE;

	RegStates	**&tbl = states_tbl[type];

	if (!tbl) {
		tbl = new RegStates *[BYTE_NUM];
		memset(tbl, 0, sizeof(RegStates *) * BYTE_NUM);
	}

	RegStates	*&sub_tbl = tbl[idx];

	if (!sub_tbl) {
		sub_tbl = new RegStates [BYTE_NUM];
		memset(sub_tbl, 0, sizeof(RegStates) * BYTE_NUM);
	}
	sub_tbl[(u_char)ch] |= state_pattern;
}

RegStates RegExp::GetRegStates(StatesType type, WCHAR ch)
{
	RegStates	**&tbl = states_tbl[type];
	RegStates *sub_tbl = tbl ? tbl[ch >> BITS_OF_BYTE] : NULL;

	return	sub_tbl ? sub_tbl[(u_char)ch] : 0;
}

void RegExp::AddEpStates(int state, const RegStates &add_states)
{
	if (!epsilon_tbl) {
		epsilon_tbl = new RegStates [sizeof(RegStates)][BYTE_NUM];
		memset(epsilon_tbl, 0, sizeof(RegStates) * sizeof(RegStates) * BYTE_NUM);
	}

	RegStates	*tbl = epsilon_tbl[state / BITS_OF_BYTE];
	int			bit = 1 << (state % BITS_OF_BYTE);

	for (int i=0; i < BYTE_NUM; i++) {
		if (i & bit)
			tbl[i] |= add_states;
	}
}

RegStates RegExp::GetEpStates(RegStates cur_states)
{
	RegStates	ret_states = 0;

	for (int i=0; cur_states; i++) {
		ret_states |= epsilon_tbl[i][cur_states & 0xff];
		cur_states >>= BITS_OF_BYTE;
	}
	return	ret_states;
}

void RegExp::AddRegStatesEx(StatesType type, WCHAR ch, RegExp::CaseSense cs)
{
	if (cs == CASE_SENSE)
		AddRegStates(type, ch, (RegStates)1 << (max_state + 1));
	else {
		AddRegStates(type, (WCHAR)CharLowerV((void *)ch), (RegStates)1 << (max_state + 1));
		AddRegStates(type, (WCHAR)CharUpperV((void *)ch), (RegStates)1 << (max_state + 1));
	}
}

BOOL RegExp::RegisterWildCard(const void *wild_str, RegExp::CaseSense cs)
{
	if (max_state + lstrlenV(wild_str) >= sizeof(RegStates) * BITS_OF_BYTE - 2)
		return	FALSE;		// start & end の２状態を除いた残り

	AddEpStates(0, (RegStates)1 << ++max_state);

	enum Mode { NORMAL, CHARCLASS } mode = NORMAL;
	enum SubMode { DEFAULT, CC_START, CC_NORMAL, CC_RANGE } submode = DEFAULT;

	int			start_state = max_state, escape = 0;
	StatesType	type = NORMAL_TBL;
	WCHAR		ch = 0, last_ch, start_ch, end_ch;

	do {
		last_ch = ch;
		ch = lGetCharIncV(&wild_str);	// 0 も文末判定文字として登録

		switch (escape) {
		case 1:  escape = 2; break;
		case 2:  escape = 0; break;
		default: if (ch == '\\') { escape = 1; continue; }
		}

		if (mode == NORMAL) {
			if (!escape) {
				switch (ch) {
				case '[':
					mode = CHARCLASS;
					submode = CC_START;
					continue;

				case '?':
					AddEpStates(max_state, (RegStates)1 << (max_state + 1));
					max_state++;
					continue;

				case '*':
					AddEpStates(max_state, (RegStates)1 << (max_state + 0));
					continue;
				}
			}
			if (ch || escape || last_ch != '*') {	// '*' で終了した場合は '\0'
				AddRegStatesEx(type, ch, cs);		// まで確認せずに判定終了させる
				max_state++;
			}
		}
		else {	// CHARCLASS mode
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
					AddRegStatesEx(type, start_ch, cs);
					start_ch++;
				}
				if (type == REV_TBL)
					AddEpStates(max_state, (RegStates)1 << (max_state + 1));
				submode = CC_NORMAL;
				continue;
			}

			if (ch == ']' && !escape) {
				mode = NORMAL;
				type = NORMAL_TBL;
				max_state++;
				continue;
			}
			if (ch == '-' && !escape) {
				submode = CC_RANGE;
				start_ch = end_ch;
				continue;
			}
			AddRegStatesEx(type, end_ch = ch, cs);
			if (type == REV_TBL)
				AddEpStates(max_state, (RegStates)1 << (max_state + 1));
		}
	} while (ch);

	end_states |= (RegStates)1 << max_state;

	return	TRUE;
}

BOOL RegExp::IsMatch(const void *target)
{
	if (!epsilon_tbl)
		return	FALSE;

	RegStates	total_states = GetEpStates(1);

	while ((total_states & end_states) == 0 && total_states) {
		WCHAR	ch = lGetCharIncV(&target);		// 0 は文末判定文字として利用

		total_states = ((total_states << 1) & GetRegStates(NORMAL_TBL, ch))
						| (GetEpStates(total_states) & ~GetRegStates(REV_TBL, ch));

		if (!ch) break;
	}
	return	(total_states & end_states) ? TRUE : FALSE;
}

