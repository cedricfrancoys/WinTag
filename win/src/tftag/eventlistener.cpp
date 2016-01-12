/* Singleton implementation of a global event listener
 
 usage:
 - declare EventListener::wndProc or EventListener::dlgProc as callback procedure for handling window messages
 - create event handling functions of type 'void eventAction(HWND hWnd)'
 - watch events you're interested in with the bind method
 
 example:
 HWND hWnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG), 0, (DLGPROC) EventListener::dlgProc, 0);
 EventListener* listener = EventListener::getInstance();
 listener->bind(hWnd, HWND_DIALOG, 0, WM_CLOSE, closeDialog);
*/
#include <Windows.h>
#include "eventlistener.h"

EventNode::EventNode(HWND hWnd = 0, int itemId = 0, UINT msg = 0, void (*f)(HWND, WPARAM, LPARAM) = NULL) {
	this->hWnd = hWnd;
	this->itemId = itemId;
	this->msg = msg;
	this->f = f;
	this->next = NULL;
}

EventListener::EventListener(UINT wndType) {
	this->first = new EventNode();
	this->wndType = wndType;
}

EventListener::~EventListener() {
	EventNode* node = this->first;
	while(node) {
		EventNode* next = node->next;
		delete node;
		node = next;
	}
}

BOOL EventListener::isDialog() {
	return this->wndType;
}

EventListener* EventListener::getInstance(UINT wndType = HWND_WINDOW) {
	static EventListener elWndInst(HWND_WINDOW), elDlgInst(HWND_DIALOG);
	switch(wndType) {
	case HWND_WINDOW:	return &elWndInst;
	case HWND_DIALOG:	return &elDlgInst;
	}
}

void EventListener::bind(HWND hWnd, int itemId, UINT msg, void (*f)(HWND, WPARAM, LPARAM)) {
	EventNode* node = new EventNode(hWnd, itemId, msg, f);
	node->next = this->first->next;
	this->first->next = node;
}

void EventListener::unbind(HWND hWnd, int itemId, UINT msg) {
	EventNode *prev = this->first, *node = prev->next;
	// search for a matching node
	while(node) {
		if(	node->hWnd == hWnd &&
			node->itemId == itemId && 
			node->msg == msg) {
			prev->next = node->next;
			delete node;
			break;
		}
		prev = node;
		node = node->next;
	}
}

LRESULT EventListener::handle(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	UINT itemId = 0;
	HWND itemHWnd = NULL;

	// todo : complete list of events that give extra-info
	switch(msg) {
	case WM_COMMAND:
		msg = HIWORD(wParam);
		itemId = LOWORD(wParam);
		itemHWnd = (HWND) lParam;
		break;
	case WM_CONTEXTMENU:
		itemHWnd = (HWND) wParam;
		break;
	}
	// search for a matching node
	EventNode* node = this->first->next;
	while(node) {
		if(node->hWnd == hWnd) {
/*
// DEBUG
			if( node->msg == msg ){
				WCHAR temp[1024];
				wsprintf(temp, L"%d:%d for %d", itemId, msg, node->itemId);
				MessageBox(hWnd, temp, L"info", MB_OK);
			}
*/
			if( node->msg == msg &&
				(
				 (node->itemId == itemId) ||
				 (itemHWnd && GetDlgItem(hWnd, node->itemId) == itemHWnd)
				)
			) {
				node->f(hWnd, wParam, lParam);
				if(this->isDialog()) return TRUE;
				return FALSE;
			}
		}
		node = node->next;
	}
	if(this->isDialog()) return FALSE;
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK EventListener::wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	EventListener* instance = EventListener::getInstance(HWND_WINDOW);
	return instance->handle(hWnd, msg, (WPARAM) wParam, (LPARAM) lParam);
}

LRESULT CALLBACK EventListener::dlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	EventListener* instance = EventListener::getInstance(HWND_DIALOG);
	return instance->handle(hWnd, msg, (WPARAM) wParam, (LPARAM) lParam);
}