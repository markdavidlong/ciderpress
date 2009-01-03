/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Simple reformatters that can apply to anything.
 */
#ifndef __LR_SIMPLE__
#define __LR_SIMPLE__

#include "ReformatBase.h"

/*
 * Fix the EOL markers on a text file.
 */
class ReformatEOL_HA : public ReformatText {
public:
	ReformatEOL_HA(void) {}
	virtual ~ReformatEOL_HA(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);
};

/*
 * Do nothing.
 */
class ReformatRaw : public Reformat {
public:
	ReformatRaw(void) {}
	virtual ~ReformatRaw(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);
};

/*
 * Reformat a block of data into a hex dump.
 */
class ReformatHexDump : public ReformatText {
public:
	ReformatHexDump(void) {}
	virtual ~ReformatHexDump(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);
};

#endif /*__LR_SIMPLE__*/