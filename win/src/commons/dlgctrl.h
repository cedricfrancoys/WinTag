/* dlgctrl.h - interface for easing creation and use of common dialog controls

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/


#ifndef __DLGCTRL_H
#define __DLGCTRL_H 1

/* Generic callback for TCN_SELCHANGE events.
*/
void DlgCtrl_EvChangeTab(HWND hWnd, WPARAM, LPARAM);

/* Create tabControl tabs and populate it with custom panes.
*/
void DlgCtrl_InitTabs(HWND hWnd, UINT nIDTabCtrl, UINT nPanesCount, UINT nIDFirstTab, DLGPROC lpDialogFunc);

/* Retrieve number of children windows (including controls).
*/
UINT DlgCtrl_CountChildren(HWND hWnd);

/* Send a message to all descendant windows.
*/
void DlgCtrl_SendChildrenMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

/* Send a message to first descendant control of given ID.
*/
int DlgCtrl_SendMessage(HWND hWnd, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam);

#endif