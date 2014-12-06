/* @(#)Copyright (C) 2005-2006 H.Shirouzu   regexp.cpp	ver1.84 */
/* ========================================================================
	Project  Name			: Regular Expression / Wild Card Match Library
	Create					: 2005-11-03(The)
	Update					: 2006-01-31(Tue)
	Reference				: 
	======================================================================== */

typedef unsigned _int64	RegStates;

class RegExp {
public:
	RegExp();
	~RegExp();

	enum CaseSense { CASE_SENSE, CASE_INSENSE };

	void	Init();
	BOOL	RegisterWildCard(const void *wild_str, CaseSense cs=CASE_SENSE);
//	BOOL	RegisterRegStr(const void *reg_str, CaseSense cs=CASE_SENSE);
	BOOL	IsMatch(const void *target);
	BOOL	IsRegistered(void) { return	max_state ? TRUE : FALSE; }

protected:
	enum StatesType { NORMAL_TBL, REV_TBL, MAX_STATES_TBL };
	RegStates	**states_tbl[MAX_STATES_TBL];
	RegStates	(*epsilon_tbl)[BYTE_NUM];
	RegStates	end_states;
	int			max_state;

	void		AddRegStates(StatesType type, WCHAR ch, const RegStates &state_pattern);
	RegStates	GetRegStates(StatesType type, WCHAR ch);
	void		AddEpStates(int state, const RegStates &add_states);
	RegStates	GetEpStates(RegStates cur_states);
	inline void AddRegStatesEx(StatesType type, WCHAR ch, CaseSense cs);
};

class RegExpEx {
public:
	RegExpEx() {
		RegArray = NULL;
		RegArrayNum = 0;
	}
	void Init() {
		while (RegArrayNum > 0) {
			delete RegArray[--RegArrayNum];
		}
		free(RegArray);
		RegArray = NULL;
	}
	BOOL	RegisterWildCard(const void *wild_str, RegExp::CaseSense cs=RegExp::CASE_SENSE) {
		int	i;
		for (i=0; i < RegArrayNum; i++) {
			if (RegArray[i]->RegisterWildCard(wild_str, cs))	return	TRUE;
			if (!RegArray[i]->IsRegistered())					return	FALSE;
		}
		RegArray = (RegExp **)realloc(RegArray, sizeof(RegExp *) * ++RegArrayNum);
		RegArray[RegArrayNum -1] = new RegExp();
		return	RegArray[i]->RegisterWildCard(wild_str, cs);
	}
	BOOL	IsMatch(const void *target) {
		for (int i=0; i < RegArrayNum; i++) {
			if (RegArray[i]->IsMatch(target))	return	TRUE;
		}
		return	FALSE;
	}
	BOOL	IsRegistered(void) { return	RegArrayNum ? TRUE : FALSE; }

protected:
	RegExp	**RegArray;
	int		RegArrayNum;
};

