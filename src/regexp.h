/* @(#)Copyright (C) 2005-2015 H.Shirouzu   regexp.cpp	ver3.00 */
/* ========================================================================
	Project  Name			: Regular Expression / Wild Card Match Library
	Create					: 2005-11-03(The)
	Update					: 2015-06-22(Mon)
	Reference				: 
	======================================================================== */

#define UINT64_BITS		64
#define MAX_REGST_NUM	32
#define MAX_REGST_STATE	(MAX_REGST_NUM * UINT64_BITS) // == 2048status

class RegStates {
public:
	RegStates(int max_state=0)  {
		st_num = 0;
		SetMaxState(max_state);
	}
	RegStates(const RegStates& org)  {
		*this = org;
	}
	~RegStates() {}

	void Reset() {
		for (int i=0; i < st_num; i++) {
			st[i] = 0;
		}
	}

	bool SetMaxState(int new_state) {
		return Grow(ALIGN_BLOCK(new_state + 1, UINT64_BITS));
	}

	bool Grow(int new_num) {
		if (new_num > MAX_REGST_NUM) return false;
		if (new_num <= st_num) return true;

		for (int i=st_num; i < new_num; i++) {
			st[i] = 0;
		}

		st_num = new_num;
		return true;
	}

	RegStates& operator |=(const RegStates &s) {
		int num = min(st_num, s.st_num);

		if (st_num < s.st_num) Grow(s.st_num);

		for (int i=0; i < num; i++) {
			st[i] |= s.st[i];
		}
		for (int i=num; i < s.st_num; i++) {
			st[i] = s.st[i];
		}

		return *this;
	}
	RegStates& operator &=(const RegStates &s) {
		int num = min(st_num, s.st_num);

		if (st_num < s.st_num) Grow(s.st_num);

		for (int i=0; i < num; i++) {
			st[i] &= s.st[i];
		}
		for (int i=num; i < st_num; i++) {
			st[i] = 0;
		}

		return *this;
	}
	RegStates& operator =(const RegStates &s) {
		for (int i=0; i < s.st_num; i++) {
			st[i] = s.st[i];
		}
		st_num = s.st_num;

		return *this;
	}

	RegStates& ShiftLeft() {
		for (int i=st_num-1; i >= 0; i--) {
			st[i] <<= 1;
			if (i > 0 && (st[i -1] & 0x8000000000000000)) st[i] |= 1;
		}
		return *this;
	}
	void GetReverse(RegStates *s) {
		int num = min(st_num, s->st_num);

		for (int i=0; i < num; i++) {
			s->st[i] = ~st[i];
		}

		// s の bit長が長い場合は ~0 で埋める（this に合わせて truncateしない）
		// GetReverse(&x); st &= x; 等の演算で不要なクリアが起きないように
		num = s->st_num;
		for (int i=st_num; i < num; i++) {
			s->st[i] = ~(uint64)0;
		}
	}
	bool HasCommonBits(const RegStates& s) const {
		int num = min(st_num, s.st_num);

		for (int i=0; i < num; i++) {
			if (st[i] & s.st[i]) return true;
		}
		return false;
	}
	bool GetBitIdx(int start_i, int *found_i) const {
		int	idx	= start_i / UINT64_BITS;
		int	off	= start_i % UINT64_BITS;

		if (idx >= st_num) return false;

		uint64	d = st[idx] >> off;

		if (!d) {
			int	i = idx+1;
			for ( ; i < st_num && !(d = st[i]); i++)
				;
			start_i = i * UINT64_BITS;
		}

		for ( ; d; start_i++, d >>= 1) {
			if (d & 1) {
				*found_i = start_i;
				return true;
			}
		}

		return false;
	}
	bool IsZero() const {
		for (int i=0; i < st_num; i++) {
			if (st[i]) return false;
		}
		return true;
	}
	void AddState(int state) {
		if (state >= st_num * UINT64_BITS) {
			if (!SetMaxState(state)) return;
		}

		int idx = state / UINT64_BITS;
		int off = state % UINT64_BITS;

		st[idx] |= ((uint64)1 << off);
	}

protected:
	uint64	st[MAX_REGST_NUM];
	int		st_num;
};


class RegExp {
public:
	RegExp();
	~RegExp();

	enum CaseSense { CASE_SENSE, CASE_INSENSE, CASE_INSENSE_SLASH };

	void	Init();
	bool	RegisterWildCard(const WCHAR *wild_str, CaseSense cs=CASE_SENSE);
//	bool	RegisterRegStr(const WCHAR *reg_str, CaseSense cs=CASE_SENSE);
	bool	IsMatch(const WCHAR *target, bool *is_mid=NULL);
	bool	IsRegistered(void) { return	max_state ? true : false; }

protected:
	enum StatesType { NORMAL_TBL, REV_TBL, MAX_STATES_TBL };
	RegStates	***states_tbl[MAX_STATES_TBL];
	RegStates	**epsilon_tbl;
	RegStates	end_states;
	int			max_state;
	int			epsilon_num;

	void		AddRegState(StatesType type, WCHAR ch, int state);
	RegStates	*GetRegStates(StatesType type, WCHAR ch);
	void		AddEpState(int state, int new_state);
	void		GetEpStates(const RegStates& cur_states, RegStates *ret_status);
	inline void AddRegStateEx(StatesType type, WCHAR ch, int state, CaseSense cs);
	bool		SetupEpTable(int append_state);
};

