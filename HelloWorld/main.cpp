// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#include <windows.h>
#include <ShObjIdl.h>
#include <d2d1.h>
#include "math.h"
#include "stdio.h"
#include <windowsx.h>
#include <sstream>
#include <string>
#include <dwrite.h>

#pragma comment(lib, "Dwrite")
#pragma comment(lib, "d2d1")

enum TTT_STATE {TTT_BLANK, TTT_RED, TTT_BLUE};
enum TTT_TURN {TTT_P1, TTT_P2};
enum TTT_GAMEOVER {TTT_P1_WIN, TTT_P2_WIN, TTT_DRAW};

template <class T> void SafeRelease(T **ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

template <class DERIVED_TYPE> 
class BaseWindow
{
public:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        DERIVED_TYPE *pThis = NULL;

        if (uMsg == WM_NCCREATE)
        {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (DERIVED_TYPE*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);

            pThis->m_hwnd = hwnd;
        }
        else
        {
            pThis = (DERIVED_TYPE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }
        if (pThis)
        {
            return pThis->HandleMessage(uMsg, wParam, lParam);
        }
        else
        {
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }

    BaseWindow() : m_hwnd(NULL) { }

    BOOL Create(
        PCWSTR lpWindowName,
        DWORD dwStyle,
        DWORD dwExStyle = 0,
        int x = CW_USEDEFAULT,
        int y = CW_USEDEFAULT,
		int nWidth = 600 + GetSystemMetrics(SM_CXSIZEFRAME) * 2,
		int nHeight = 600 + GetSystemMetrics(SM_CYSIZEFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION),
        HWND hWndParent = 0,
        HMENU hMenu = 0
        )
    {
        WNDCLASS wc = {0};

        wc.lpfnWndProc   = DERIVED_TYPE::WindowProc;
        wc.hInstance     = GetModuleHandle(NULL);
        wc.lpszClassName = ClassName();

        RegisterClass(&wc);

        m_hwnd = CreateWindowEx(
            dwExStyle, ClassName(), lpWindowName, dwStyle, x, y,
            nWidth, nHeight, hWndParent, hMenu, GetModuleHandle(NULL), this
            );

        return (m_hwnd ? TRUE : FALSE);
    }

    HWND Window() const { return m_hwnd; }

protected:

    virtual PCWSTR  ClassName() const = 0;
    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;

    HWND m_hwnd;
};

class MainWindow : public BaseWindow<MainWindow> {
	ID2D1Factory *pFactory;
	ID2D1HwndRenderTarget *pRenderTarget;
	ID2D1SolidColorBrush *pBrushBg;
	ID2D1SolidColorBrush *pBrushBlack;
	ID2D1SolidColorBrush *pBrushRed;
	ID2D1SolidColorBrush *pBrushBlue;
	IDWriteFactory *pDWriteFactory;
	IDWriteTextFormat *pTextFormat;
	D2D1_RECT_F vert_line_1;
	D2D1_RECT_F vert_line_2;
	D2D1_RECT_F horz_line_1;
	D2D1_RECT_F horz_line_2;
	D2D1_RECT_F squares[9];
	TTT_STATE square_state[9];
	FLOAT square_size;
	TTT_TURN turn;
	int turns_left;

	void CalculateLayout();
	HRESULT CreateGraphicsResources();
	void DiscardGraphicsResources();
	void OnPaint();
	void Resize();
	int GetSelectedSquare(int xPos, int yPos);
	void ResetGame();
	void GameOver(TTT_GAMEOVER gameover);
	BOOL CheckForWin(int selected_square, TTT_STATE state);

public:

	MainWindow() : pFactory(NULL), pRenderTarget(NULL), pBrushBg(NULL),
					pBrushBlack(NULL), pBrushRed(NULL), pBrushBlue(NULL),
					pDWriteFactory(NULL), pTextFormat(NULL) {
		for(int i = 0; i < 9; i++) {
			square_state[i] = TTT_BLANK;
		}

		turn = TTT_P1;

		turns_left = 9;
	}

	PCWSTR ClassName() const { return L"Sample Window Class"; }
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

void MainWindow::CalculateLayout() {
	if (pRenderTarget != NULL) {
		vert_line_1 = D2D1::RectF(195.0f, 0, 205.0f, 600.0f);
		vert_line_2 = D2D1::RectF(395.0f, 0, 405.0f, 600.0f);
		horz_line_1 = D2D1::RectF(0, 195.0f, 600.0f, 205.0f);
		horz_line_2 = D2D1::RectF(0, 395.0f, 600.0f, 405.0f);
		for (int i = 0; i < 9; i++) {
			squares[i] = D2D1::RectF(200*(i%3), 200*(i/3), 200*(i%3+1), 200*(i/3+1));
		}
	}
}

HRESULT MainWindow::CreateGraphicsResources() {
	HRESULT hr = S_OK;
	static const WCHAR msc_fontName[] = L"Verdana";
    static const FLOAT msc_fontSize = 50;

	if (pRenderTarget == NULL) {
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		hr = pFactory->CreateHwndRenderTarget(
				D2D1::RenderTargetProperties(),
				D2D1::HwndRenderTargetProperties(m_hwnd, size),
				&pRenderTarget);

		if (SUCCEEDED(hr)) {
			const D2D1_COLOR_F color_bg = D2D1::ColorF(1.0f, 1.0f, 1.0f);
			const D2D1_COLOR_F color_black = D2D1::ColorF(0, 0, 0);
			const D2D1_COLOR_F color_red = D2D1::ColorF(1.0f, 0, 0);
			const D2D1_COLOR_F color_blue = D2D1::ColorF(0, 0, 1.0f);
			hr = pRenderTarget->CreateSolidColorBrush(color_bg, &pBrushBg);
			hr = pRenderTarget->CreateSolidColorBrush(color_black, &pBrushBlack);
			hr = pRenderTarget->CreateSolidColorBrush(color_red, &pBrushRed);
			hr = pRenderTarget->CreateSolidColorBrush(color_blue, &pBrushBlue);

			if (SUCCEEDED(hr)) {
				hr = DWriteCreateFactory(
					DWRITE_FACTORY_TYPE_SHARED,
					__uuidof(pDWriteFactory),
					reinterpret_cast<IUnknown **>(&pDWriteFactory)
					);
			}
			
			if (SUCCEEDED(hr)) {
				hr = pDWriteFactory->CreateTextFormat(
					msc_fontName,
					NULL,
					DWRITE_FONT_WEIGHT_NORMAL,
					DWRITE_FONT_STYLE_NORMAL,
					DWRITE_FONT_STRETCH_NORMAL,
					msc_fontSize,
					L"", //locale
					&pTextFormat
				);
			}

			if (SUCCEEDED(hr)) {
				// Center the text horizontally and vertically.
				pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
				pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
				CalculateLayout();
			}
		}
	}
	return hr;
}

void MainWindow::DiscardGraphicsResources() {
	SafeRelease(&pRenderTarget);
	SafeRelease(&pBrushBg);
	SafeRelease(&pBrushBlack);
	SafeRelease(&pBrushRed);
	SafeRelease(&pBrushBlue);
	SafeRelease(&pDWriteFactory);
	SafeRelease(&pTextFormat);
}

void MainWindow::OnPaint() {
	HRESULT hr = CreateGraphicsResources();
	if (SUCCEEDED(hr)) {
		PAINTSTRUCT ps;
		BeginPaint(m_hwnd, &ps);

		pRenderTarget->BeginDraw();

		pRenderTarget->Clear( D2D1::ColorF(D2D1::ColorF::White) );
	
		for (int i = 0; i < 9; i++) {
			switch (square_state[i]) {
			case TTT_RED:
				pRenderTarget->FillRectangle(squares[i], pBrushRed);
				break;
			case TTT_BLUE:
				pRenderTarget->FillRectangle(squares[i], pBrushBlue);
				break;
			case TTT_BLANK:
			default:
				pRenderTarget->FillRectangle(squares[i], pBrushBg);
				break;
			}
		}

		pRenderTarget->FillRectangle(vert_line_1, pBrushBlack);
		pRenderTarget->FillRectangle(vert_line_2, pBrushBlack);
		pRenderTarget->FillRectangle(horz_line_1, pBrushBlack);
		pRenderTarget->FillRectangle(horz_line_2, pBrushBlack);

		hr = pRenderTarget->EndDraw();
		
		if(FAILED(hr) || hr== D2DERR_RECREATE_TARGET) {
			DiscardGraphicsResources();
		}

		EndPaint(m_hwnd, &ps);
	}
}

void MainWindow::Resize() {
	if (pRenderTarget != NULL) {
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		pRenderTarget->Resize(size);
		CalculateLayout();
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
}

int MainWindow::GetSelectedSquare(int xPos, int yPos) {
	if (yPos < 0 || yPos > 600 || xPos < 0 || xPos > 600) return -1;
	else if (yPos < 200) {
		if (xPos < 200) return 0;
		else if (xPos < 400) return 1;
		else return 2;
	} else if (yPos < 400) {
		if (xPos < 200) return 3;
		else if (xPos < 400) return 4;
		else return 5;
	} else if (yPos < 600) {
		if (xPos < 200) return 6;
		else if (xPos < 400) return 7;
		else return 8;
	} else return -1;
}

void MainWindow::ResetGame() {
	for (int i = 0; i < 9; i++) {
		square_state[i] = TTT_BLANK;
	}

	turn = TTT_P1;
	turns_left = 9;
}

void MainWindow::GameOver(TTT_GAMEOVER gameover) {
	Sleep(250);
	std::string msg_str = "";
	
	switch (gameover) {
	case TTT_P1_WIN:
		msg_str = "Player 1 Wins!!";
		break;
	case TTT_P2_WIN:
		 msg_str = "Player 2 Wins!!";
		break;
	case TTT_DRAW:
		 msg_str = "DRAW =[";
		break;
	}
	
	WCHAR msg[100];
	
	swprintf(msg, 100, L"%hs", msg_str.c_str());
	D2D1_SIZE_F renderTargetSize = pRenderTarget->GetSize();
	HRESULT hr = CreateGraphicsResources();
	if (SUCCEEDED(hr)) {
		pRenderTarget->BeginDraw();
		pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
		pRenderTarget->DrawTextW(
			msg,
			msg_str.length() + 1,
			pTextFormat,
			D2D1::RectF(0, 0, renderTargetSize.width, renderTargetSize.height),
			pBrushBlack
		);
		hr = pRenderTarget->EndDraw();

		if (hr == D2DERR_RECREATE_TARGET) {
            hr = S_OK;
            DiscardGraphicsResources();
        }
	}

	Sleep(2000);
	ResetGame();
	OnPaint();
}

BOOL MainWindow::CheckForWin(int selected_square, TTT_STATE state) {
	switch (selected_square) {
	case 0:
		if (square_state[1] == state && square_state[2] == state) return TRUE;
		else if (square_state[4] == state && square_state[8] == state) return TRUE;
		else if (square_state[3] == state && square_state[6] == state) return TRUE;
		else return FALSE;
	case 1:
		if (square_state[0] == state && square_state[2] == state) return TRUE;
		else if (square_state[4] == state && square_state[7] == state) return TRUE;
		else return FALSE;
	case 2:
		if (square_state[1] == state && square_state[0] == state) return TRUE;
		else if (square_state[4] == state && square_state[6] == state) return TRUE;
		else if (square_state[5] == state && square_state[8] == state) return TRUE;
		else return FALSE;
	case 3:
		if (square_state[0] == state && square_state[6] == state) return TRUE;
		else if (square_state[4] == state && square_state[5] == state) return TRUE;
		else return FALSE;
	case 4:
		if (square_state[0] == state && square_state[8] == state) return TRUE;
		else if (square_state[1] == state && square_state[7] == state) return TRUE;
		else if (square_state[2] == state && square_state[6] == state) return TRUE;
		else if (square_state[3] == state && square_state[5] == state) return TRUE;
		else return FALSE;
	case 5:
		if (square_state[8] == state && square_state[2] == state) return TRUE;
		else if (square_state[3] == state && square_state[4] == state) return TRUE;
		else return FALSE;
	case 6:
		if (square_state[0] == state && square_state[3] == state) return TRUE;
		else if (square_state[4] == state && square_state[2] == state) return TRUE;
		else if (square_state[7] == state && square_state[8] == state) return TRUE;
		else return FALSE;
	case 7:
		if (square_state[1] == state && square_state[4] == state) return TRUE;
		else if (square_state[6] == state && square_state[8] == state) return TRUE;
		else return FALSE;
	case 8:
		if (square_state[2] == state && square_state[5] == state) return TRUE;
		else if (square_state[4] == state && square_state[0] == state) return TRUE;
		else if (square_state[6] == state && square_state[7] == state) return TRUE;
		else return FALSE;
	default:
		return FALSE;
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{

	MainWindow win;

	if(!win.Create(L"Tic Tac Toe", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME)) {
		return 0;
	}

	ShowWindow(win.Window(), nCmdShow);

    // Run the message loop.

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int xPos = 0;
	int yPos = 0;
	int selected_square;

	std::stringstream debugoutput;

    switch (uMsg)
    {
	case WM_LBUTTONUP:
		xPos = GET_X_LPARAM(lParam);
		yPos = GET_Y_LPARAM(lParam);
		
		debugoutput << "The coords are " << xPos << ", " << yPos << std::endl;
		OutputDebugStringA(debugoutput.str().c_str());
		debugoutput.flush();

		selected_square = GetSelectedSquare(xPos, yPos);

		debugoutput << "The selected square is " << selected_square;
		OutputDebugStringA(debugoutput.str().c_str());
		debugoutput.flush();

		if (selected_square == -1) return 0;
		else if (square_state[selected_square] == TTT_BLANK) {
			if (turn == TTT_P1) {
				square_state[selected_square] = TTT_RED;
				OnPaint();
				if (CheckForWin(selected_square, TTT_RED)) {
					GameOver(TTT_P1_WIN);
					return 0;
				}
				turn = TTT_P2;
			} else {
				square_state[selected_square] = TTT_BLUE;
				OnPaint();
				if (CheckForWin(selected_square, TTT_BLUE)) {
					GameOver(TTT_P2_WIN);
					return 0;
				}
				turn = TTT_P1;
			}
			if (--turns_left == 0) {
				GameOver(TTT_DRAW);
				return 0;
			}
		}
		return 0;

	case WM_CREATE:
		if (FAILED(D2D1CreateFactory(
				D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory))) {
			return -1;
		}
		return 0;

    case WM_DESTROY:
        DiscardGraphicsResources();
		SafeRelease(&pFactory);
		PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

	case WM_SIZE:
		Resize();
		return 0;

    default:
        return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
    }

    return TRUE;
}
