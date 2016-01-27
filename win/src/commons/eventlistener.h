/* eventlistener.h - Singleton implementation of a global event listener.

    This file is part of the tagger-ui suite <http://www.github.com/cedricfrancoys/tagger-ui>
    Copyright (C) Cedric Francoys, 2016, Yegen
    Some Right Reserved, GNU GPL 3 license <http://www.gnu.org/licenses/>
*/


#define HWND_WINDOW 0
#define HWND_DIALOG	1


class EventNode {
public:
	HWND hWnd;
	int itemId;
	UINT msg;
	void (*f)(HWND, WPARAM, LPARAM);
	EventNode* next;
	EventNode(HWND, int, UINT, void (*)(HWND, WPARAM, LPARAM));
};

class EventListener {
	EventNode* first;
	UINT wndType;
	EventListener(UINT);
	~EventListener();
	LRESULT handle(HWND, UINT, WPARAM, LPARAM);
	BOOL isDialog();
public:
	static EventListener* getInstance(UINT);
	static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
	static LRESULT CALLBACK dlgProc(HWND, UINT, WPARAM, LPARAM);
	void bind(HWND, int, UINT, void (*)(HWND, WPARAM, LPARAM));
	void unbind(HWND, int, UINT);
};