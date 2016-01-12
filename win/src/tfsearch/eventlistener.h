#define HWND_WINDOW 0
#define HWND_DIALOG	1


/* Singleton implementation of a global event listener
 
 usage:
 - declare EventListener::listen as callback procedure for handling window messages
 - create event handling functions of type 'void eventAction(HWND hWnd)'
 - watch event you're interested in with the bind method
 
 example:
 HWND hWnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG), 0, (DLGPROC) EventListener::listen, 0);
 EventListener* listener = EventListener::getInstance();
 listener->bind(hWnd, HWND_DIALOG, IDD_DIALOG, WM_CLOSE, closeDialog);
*/


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