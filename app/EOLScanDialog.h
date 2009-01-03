/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * A simple dialog to display the results of an EOL scan.
 */
#ifndef __EOLSCANDIALOG__
#define __EOLSCANDIALOG__

#include "resource.h"

/*
 * Entire class is here.
 */
class EOLScanDialog : public CDialog {
public:
	EOLScanDialog(CWnd* pParentWnd = NULL) :
		CDialog(IDD_EOLSCAN, pParentWnd)
		{}
	virtual ~EOLScanDialog(void) {}

	long	fCountChars;
	long	fCountCR;
	long	fCountLF;
	long	fCountCRLF;
	long	fCountHighASCII;

private:
	BOOL OnInitDialog(void);
	afx_msg void OnHelp(void);

	DECLARE_MESSAGE_MAP()
};

#endif /*__EOLSCANDIALOG__*/