#pragma comment(linker,"\"/manifestdependency:type='win32' \
 name='Microsoft.Windows.Common-Controls' \
 version='6.0.0.0' processorArchitecture='*' \
 publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

#include <Windows.h>
#include <stdio.h>
#include <shlwapi.h>
#include <Uxtheme.h>
#include <math.h>

#include "theme_manager.h"
#include "resource.h"

typedef struct {
	HCURSOR hCursor;
	WCHAR fullPath[MAX_PATH];
} Cursor;

HWND g_themesListView;
HWND g_applyButton;
HWND g_removeButton;
HINSTANCE g_hInstance;
Themes g_themes = { 0 };
TCHAR g_baseDir[MAX_PATH];

Theme defaultTheme = {
	.name = L"Windows Default",
	.BasePath = L"",
	.description = L"Default windows theme",
	.index = 0,
	.Icons = {
		L"%SystemRoot%\\cursors\\aero_arrow.cur",
		L"%SystemRoot%\\cursors\\aero_helpsel.cur",
		L"%SystemRoot%\\cursors\\aero_working.ani",
		L"%SystemRoot%\\cursors\\aero_busy.ani",
		L"", // crosshair
		L"", // text  
		L"%SystemRoot%\\cursors\\aero_pen.cur",
		L"%SystemRoot%\\cursors\\aero_unavail.cur",
		L"%SystemRoot%\\cursors\\aero_ns.cur",
		L"%SystemRoot%\\cursors\\aero_ew.cur",
		L"%SystemRoot%\\cursors\\aero_nwse.cur",
		L"%SystemRoot%\\cursors\\aero_nesw.cur",
		L"%SystemRoot%\\cursors\\aero_move.cur",
		L"%SystemRoot%\\cursors\\aero_up.cur",
		L"%SystemRoot%\\cursors\\aero_link.cur",
		L"%SystemRoot%\\cursors\\aero_pin.cur",
		L"%SystemRoot%\\cursors\\aero_person.cur",
	},
};

Theme g_selectedTheme = { 0 };
Cursor g_Cursors[17] = { 0 };
RECT g_iconsRects[17] = { 0 };
HCURSOR g_hoveredCursor = NULL;

void RefreshThemeList(HWND hWnd, BOOL showMessageBox);

BOOL GetUniqueTempFilePath(TCHAR* outPath, DWORD maxLen) {
	TCHAR tempPath[MAX_PATH];
	DWORD pathLen = GetTempPathW(maxLen, tempPath);
	if (pathLen == 0 || pathLen > maxLen)
		return FALSE;

	UINT uRet = GetTempFileNameW(tempPath, L"CSY", 0, outPath);
	if (uRet == 0)
		return FALSE;

	return TRUE;
}

void ShowErrorMessage(HWND hWnd, const wchar_t* message, const wchar_t* title) {
	MessageBoxW(hWnd, message, title, MB_OK | MB_ICONERROR);
}

void ShowInfoMessage(HWND hWnd, const wchar_t* message, const wchar_t* title) {
	MessageBoxW(hWnd, message, title, MB_OK | MB_ICONINFORMATION);
}

void ImportFiles(HWND hWnd) {
	OPENFILENAMEW ofn;
	TCHAR* szFile = malloc(MAX_PATH * 50 * sizeof(TCHAR));

	if (!szFile) {
		ShowErrorMessage(hWnd, L"Couldn't allocate buffer for files", L"Error");
		return;
	}

	ZeroMemory(szFile, MAX_PATH * 50 * sizeof(TCHAR));
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH * 50;
	ofn.lpstrFilter = L"Archive Files (*.gz;*.xz;*.tar;*.bz2;*.zip)\0*.gz;*.xz;*.tar;*.bz2;*.zip\0All Files\0*.*\0\0";
	ofn.nFilterIndex = 0;
	ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileNameW(&ofn)) {
		TCHAR* startDir = NULL;
		TCHAR* start = szFile;
		TCHAR* end = szFile;

		if (*(start + wcslen(start) + 1) != '\0') {
			startDir = start;
			start += wcslen(start) + 1;
			end = start;
		}

		size_t initialThemeCount = g_themes.length;
		size_t successCount = 0;
		size_t errorCount = 0;

		HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

		while (*start != '\0') {
			while (*end != '\0') end++;
			end++;

			TCHAR fullPath[MAX_PATH];
			if (startDir) {
				swprintf_s(fullPath, MAX_PATH, L"%s\\%s", startDir, start);
			}
			else {
				wcscpy_s(fullPath, MAX_PATH, start);
			}

			ThemeResult result = ThemeManager_ImportFromArchive(fullPath, &g_themes);

			if (result == THEME_RESULT_SUCCESS) {
				successCount++;
			}
			else {
				errorCount++;
				TCHAR errorMsg[512];
				swprintf_s(errorMsg, 512, L"Failed to import '%s': %s",
					PathFindFileNameW(fullPath), ThemeManager_GetErrorMessage(result));
				ShowErrorMessage(hWnd, errorMsg, L"Import Error");
			}

			start += end - start;
		}

		SetCursor(hOldCursor);

		if (successCount > 0 || errorCount > 0) {
			size_t totalNewThemes = g_themes.length - initialThemeCount;
			TCHAR summaryMsg[512];

			if (errorCount == 0) {
				swprintf_s(summaryMsg, 512, L"Successfully imported %zu theme(s) from %zu archive(s).",
					totalNewThemes, successCount);
				ShowInfoMessage(hWnd, summaryMsg, L"Import Complete");
			}
			else {
				swprintf_s(summaryMsg, 512, L"Import completed with %zu success(es) and %zu error(s).\n%zu theme(s) imported.",
					successCount, errorCount, totalNewThemes);
				ShowInfoMessage(hWnd, summaryMsg, L"Import Results");
			}
		}

		RefreshThemeList(hWnd, FALSE);
	}

	free(szFile);
}

void ImportFromFolder(HWND hWnd) {
	OPENFILENAMEW ofn = { 0 };
	wchar_t fileName[MAX_PATH] = L"";
	wchar_t folderPath[MAX_PATH];

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = L"Theme Files (*.theme;*.inf)\0*.theme;*.inf\0All Files\0*.*\0\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrTitle = L"Select any file in the folder containing cursor theme";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	if (GetOpenFileNameW(&ofn)) {
		wcscpy_s(folderPath, MAX_PATH, fileName);
		wchar_t* lastSlash = wcsrchr(folderPath, L'\\');
		if (lastSlash) {
			*lastSlash = L'\0';
		}

		HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

		size_t initialCount = g_themes.length;
		ThemeResult result = ThemeManager_ImportFromDirectory(folderPath, &g_themes);

		SetCursor(hOldCursor);

		if (result == THEME_RESULT_SUCCESS) {
			size_t newThemes = g_themes.length - initialCount;
			if (newThemes > 0) {
				wchar_t msg[256];
				swprintf_s(msg, 256, L"Successfully imported %zu theme(s) from folder.", newThemes);
				ShowInfoMessage(hWnd, msg, L"Import Complete");
			}
			else {
				ShowInfoMessage(hWnd, L"No valid cursor themes found in the selected folder.", L"Import Results");
			}
		}
		else {
			wchar_t errorMsg[512];
			swprintf_s(errorMsg, 512, L"Failed to import from folder: %s",
				ThemeManager_GetErrorMessage(result));
			ShowErrorMessage(hWnd, errorMsg, L"Import Error");
		}

		RefreshThemeList(hWnd, FALSE);
	}
}

void RefreshThemeList(HWND hWnd, BOOL showMessageBox)
{
	g_themes.length = 0;
	ListView_DeleteAllItems(g_themesListView);
	da_append(&g_themes, defaultTheme);

	wchar_t searchPath[MAX_PATH];
	wsprintf(searchPath, L"%s\\themes\\*", g_baseDir);

	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW(searchPath, &findData);

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
				continue;

			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;

			wchar_t themeIniPath[MAX_PATH];
			wsprintf(themeIniPath, L"%s\\themes\\%s\\theme.ini", g_baseDir, findData.cFileName);

			if (GetFileAttributesW(themeIniPath) == INVALID_FILE_ATTRIBUTES)
				continue;

			Theme theme = { 0 };
			theme.index = g_themes.length;

			ThemeManager_ProcessTheme(themeIniPath, &theme);

			if (!theme.name)
				theme.name = _wcsdup(findData.cFileName);

			wsprintf(theme.BasePath, L"%s\\themes\\%s", g_baseDir, findData.cFileName);

			da_append(&g_themes, theme);

		} while (FindNextFileW(hFind, &findData));

		FindClose(hFind);
	}

	for (int i = 0; i < g_themes.length; i++)
	{
		LVITEM lvi = { 0 };
		lvi.mask = LVIF_TEXT;
		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.pszText = g_themes.items[i].name;
		ListView_InsertItem(g_themesListView, &lvi);
	}

	if (showMessageBox)
		ShowInfoMessage(hWnd, L"Succesfully refreshed themes list", L"Success");
}

void ShowAbout(HWND hWnd) {
	MessageBoxW(hWnd,
		L"Cursory - Windows Cursor Theme Manager\n\n"
		L"A lightweight and fast cursor manager for Windows.\n"
		L"Convert and apply XCursor themes with ease.\n\n"
		L"Features:\n"
		L"• Import XCursor and Windows cursor themes\n"
		L"• Support for compressed archives\n"
		L"• One-click theme application\n"
		L"• Automatic cursor conversion",
		L"About Cursory",
		MB_OK | MB_ICONINFORMATION);
}

BOOL GetIconSize(HICON hIcon, int* width, int* height)
{
	if (!hIcon || !width || !height)
		return FALSE;

	ICONINFO iconInfo;
	if (!GetIconInfo(hIcon, &iconInfo))
		return FALSE;

	BITMAP bmp;
	BOOL result = GetObject(iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask, sizeof(BITMAP), &bmp);

	if (result)
	{
		*width = bmp.bmWidth;
		*height = bmp.bmHeight;

		if (!iconInfo.hbmColor)
			*height /= 2;
	}

	if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
	if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);

	return result;
}

void DrawIconStretch(HDC hdc, HICON hIcon, int x, int y, int maxWidth, int maxHeight)
{
	if (!hdc || !hIcon || maxWidth <= 0 || maxHeight <= 0)
		return;

	int iconWidth, iconHeight;
	if (!GetIconSize(hIcon, &iconWidth, &iconHeight))
	{
		iconWidth = iconHeight = 32;
	}

	int drawWidth = iconWidth;
	int drawHeight = iconHeight;

	/*if (drawWidth < 64 || drawHeight < 64)
	{
		float scaleX = 64.0f / drawWidth;
		float scaleY = 64.0f / drawHeight;
		float scale = max(scaleX, scaleY);

		drawWidth = (int)(drawWidth * scale);
		drawHeight = (int)(drawHeight * scale);
	}*/

	if (drawWidth > maxWidth || drawHeight > maxHeight)
	{
		float scaleX = (float)maxWidth / drawWidth;
		float scaleY = (float)maxHeight / drawHeight;
		float scale = min(scaleX, scaleY);

		drawWidth = (int)(drawWidth * scale);
		drawHeight = (int)(drawHeight * scale);
	}

	int centerX = x + (maxWidth - drawWidth) / 2;
	int centerY = y + (maxHeight - drawHeight) / 2;

	int oldStretchMode = SetStretchBltMode(hdc, HALFTONE);

	POINT ptOld;
	SetBrushOrgEx(hdc, 0, 0, &ptOld);

	DrawIconEx(hdc, centerX, centerY, hIcon, drawWidth, drawHeight, 0, NULL, DI_NORMAL);

	SetBrushOrgEx(hdc, ptOld.x, ptOld.y, NULL);
	SetStretchBltMode(hdc, oldStretchMode);
}

void ApplyTheme(Theme theme)
{
	HKEY hKey;
	DWORD type = 0, size = 0;

	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Control Panel\\Cursors", 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
		return;

	if (RegSetValueEx(hKey, NULL, 0, REG_SZ, (const BYTE*)theme.name, (DWORD)(wcslen(theme.name) * sizeof(wchar_t) + 1)) != ERROR_SUCCESS)
		return;

	TCHAR tmp[1024];
	TCHAR result[MAX_PATH * 17] = L"";
	for (size_t i = 0; i < 17; i++)
	{
		if (theme.Icons[i][0] != '%')
			wsprintf(tmp, L"%s\\cursors\\%s", theme.BasePath, theme.Icons[i]);
		else
			wcscpy(tmp, theme.Icons[i]);

		if (wcslen(theme.Icons[i]) == 0)
			memset(tmp, 0, 1024 * sizeof(TCHAR));

		if (RegSetValueExW(hKey, ThemeManager_IconIndexToName(i), 0, REG_EXPAND_SZ,
			(const BYTE*)tmp,
			(DWORD)((wcslen(tmp) + 1) * sizeof(wchar_t))) != ERROR_SUCCESS)
			return;

		wcscat(result, tmp);

		if (i < 16)
			wcscat(result, L",");
	}


	if (theme.index != 0)
	{
		DWORD type = 1;
		DWORD dwDisposition;
		RegSetValueExW(hKey, L"Scheme Source", 0, REG_DWORD, (const BYTE*)&type, sizeof(DWORD));

		RegCloseKey(hKey);

		if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Control Panel\\Cursors\\Schemes", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisposition) != ERROR_SUCCESS)
			return;

		if (RegSetValueEx(hKey, theme.name, 0, REG_EXPAND_SZ, (const BYTE*)result, (DWORD)(wcslen(result) * sizeof(wchar_t) + 1)) != ERROR_SUCCESS)
			return;

		RegCloseKey(hKey);
	}
	else
	{
		DWORD type = 2;
		RegSetValueExW(hKey, L"Scheme Source", 0, REG_DWORD, (const BYTE*)&type, sizeof(DWORD));

		RegCloseKey(hKey);
	}

	SystemParametersInfo(SPI_SETCURSORS, 0, NULL, 0);

	wsprintf(tmp, L"Successfully set cursor theme to '%s'", theme.name);
	MessageBox(NULL, tmp, L"Success", MB_OK | MB_ICONINFORMATION);
}

void RemoveSelectedTheme(HWND hWnd)
{
	int index = ListView_GetNextItem(g_themesListView, -1, LVNI_SELECTED);

	if (index == 0)
	{
		ShowErrorMessage(hWnd, L"Can't delete default theme", L"Error");
		return;
	}

	da_remove(&g_themes, index);
	ThemeManager_DeleteDirectoryRecursive(g_selectedTheme.BasePath);
	g_selectedTheme = (Theme){ 0 };
	ListView_SetItemState(g_themesListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	RefreshThemeList(hWnd, FALSE);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static TCHAR tmp[1024];
	static int hoveredIcon = -1;
	static const LONG MAIN_TITLE_HEIGHT = 35;
	static const LONG THEME_NAME_HEIGHT = 20;
	static const LONG THEME_ICONS_HEIGHT = 16;
	static const LONG THEME_INFO_HEIGHT = 15;

	switch (uMsg) {
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		RECT listViewRect = { 0 };
		GetWindowRect(g_themesListView, &listViewRect);
		MapWindowPoints(HWND_DESKTOP, hWnd, (LPPOINT)&listViewRect, 2);
		RECT clientRect = { 0 };
		GetClientRect(hWnd, &clientRect);

		LOGFONT lf = { 0 };
		HFONT hUiFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		GetObject(hUiFont, sizeof(LOGFONT), &lf);
		lf.lfHeight = THEME_NAME_HEIGHT;
		HFONT hFont = CreateFontIndirect(&lf);
		HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

		RECT textRect = (RECT){
			.left = listViewRect.right,
			.right = clientRect.right,
			.bottom = clientRect.bottom,
			.top = clientRect.top + 10,
		};

		if (g_selectedTheme.name)
		{
			DrawText(hdc, g_selectedTheme.name, -1, &textRect, DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
			textRect.top += THEME_NAME_HEIGHT;

			DeleteObject(hFont);
			lf.lfHeight = THEME_INFO_HEIGHT;
			hFont = CreateFontIndirect(&lf);
			SelectObject(hdc, hFont);

			if (g_selectedTheme.description) {
				DrawText(hdc, L"Description:", 13, &textRect, DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
				textRect.top += THEME_INFO_HEIGHT;
				DrawText(hdc, g_selectedTheme.description, -1, &textRect, DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
				textRect.top += THEME_INFO_HEIGHT;
			}

#ifdef _DEBUG
			if (g_selectedTheme.BasePath) {
				DrawText(hdc, g_selectedTheme.BasePath, -1, &textRect, DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
				textRect.top += THEME_INFO_HEIGHT;
			}
#endif
			SIZE btnSize = { 0 };
			Button_GetIdealSize(g_applyButton, &btnSize);

			MoveToEx(hdc, textRect.left, textRect.top + 5, NULL);
			LineTo(hdc, textRect.right, textRect.top + 5);

			MoveToEx(hdc, textRect.left, textRect.bottom - btnSize.cy - 5, NULL);
			LineTo(hdc, textRect.right, textRect.bottom - btnSize.cy - 5);

			RECT leftSize = {
			.left = textRect.left + 5,
			.right = textRect.right - 5,
			.bottom = textRect.bottom - btnSize.cy - 10,
			.top = textRect.top + 10,
			};

			LONG leftWidth = leftSize.right - leftSize.left;
			LONG leftHeight = leftSize.bottom - leftSize.top;

			int amountOfIcons = 17;

			int margin = 8;
			int textMargin = 5;
			int textHeight = THEME_ICONS_HEIGHT;

			DeleteObject(hFont);
			lf.lfHeight = THEME_ICONS_HEIGHT;
			hFont = CreateFontIndirect(&lf);
			SelectObject(hdc, hFont);

			HPEN hPen = CreatePen(PS_SOLID, 2, 0);
			HGDIOBJ oldPen = SelectObject(hdc, hPen);

			int cols = (int)ceil(sqrt((double)amountOfIcons * leftWidth / leftHeight));
			int rows = (int)ceil((double)amountOfIcons / cols);

			int cellWidth = (int)round((leftWidth - (cols + 1.0) * margin) / cols);
			int cellHeight = (int)round((leftHeight - (rows + 1.0) * margin) / rows);

			int rectangleSize = min(cellWidth, cellHeight - textMargin - textHeight);

			if (rectangleSize > 0) {
				int item = 0;
				for (int r = 0; r < rows && item < amountOfIcons; r++) {
					int remaining = amountOfIcons - item;
					int itemsInRow = (remaining < cols) ? remaining : cols;

					int totalRowWidth = itemsInRow * (cellWidth + margin) - margin;
					int rowOffset = (cols * (cellWidth + margin) - margin - totalRowWidth) / 2;

					for (int c = 0; c < itemsInRow && item < amountOfIcons; c++, item++) {
						int cellX = (int)(leftSize.left + margin + rowOffset + c * (cellWidth + margin));
						int cellY = (int)(leftSize.top + margin + r * (cellHeight + margin));
						int imgX = cellX + (int)((cellWidth - rectangleSize) / 2.0);
						int imgY = cellY;

						Rectangle(hdc, imgX, imgY, imgX + rectangleSize, imgY + rectangleSize);

						g_iconsRects[item] = (RECT){
							.left = imgX,
							.right = imgX + rectangleSize,
							.bottom = imgY + rectangleSize,
							.top = imgY,
						};

						DrawIconStretch(hdc, g_Cursors[item].hCursor, imgX + 5, imgY + 5, rectangleSize - 10, rectangleSize - 10);

						RECT txt = {
							.left = imgX,
							.top = imgY + rectangleSize + (int)textMargin,
							.right = imgX + rectangleSize,
							.bottom = imgY + rectangleSize + (int)(textMargin + textHeight)
						};
						DrawText(hdc, ThemeManager_IconIndexToName(item), -1, &txt,
							DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
					}
				}
			}

			SelectObject(hdc, oldPen);
			DeleteObject(hPen);
		}
		else
		{
			DeleteObject(hFont);
			lf.lfHeight = MAIN_TITLE_HEIGHT;
			hFont = CreateFontIndirect(&lf);
			SelectObject(hdc, hFont);

			textRect.top = (clientRect.bottom - clientRect.top) / 2 - MAIN_TITLE_HEIGHT;
			textRect.bottom = textRect.top + MAIN_TITLE_HEIGHT;

			DrawText(hdc, L"Cursory", -1, &textRect,
				DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);

			textRect.top += MAIN_TITLE_HEIGHT;
			textRect.bottom += THEME_NAME_HEIGHT;
			DeleteObject(hFont);
			lf.lfHeight = THEME_NAME_HEIGHT;
			hFont = CreateFontIndirect(&lf);
			SelectObject(hdc, hFont);

			DrawText(hdc, L"Lightweight & fast cursor manager for Windows", -1, &textRect,
				DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
		}

		DeleteObject(hFont);
		SelectObject(hdc, hOldFont);

		EndPaint(hWnd, &ps);
	}
	break;
	case WM_MOUSEMOVE:
	{
		if (!g_selectedTheme.name)
		{
			if (hoveredIcon != -1)
				hoveredIcon = -1;

			break;
		}

		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(hWnd, &pt);

		int newHover = -1;
		for (size_t i = 0; i < 17; i++)
		{
			if (PtInRect(&g_iconsRects[i], pt)) {
				newHover = (int)i;
				break;
			}
		}

		if (newHover != hoveredIcon)
		{
			DestroyCursor(g_hoveredCursor);
			hoveredIcon = newHover;
			g_hoveredCursor = NULL;
			if (hoveredIcon != -1)
			{
				HCURSOR hCursor = (HCURSOR)LoadImage(
					NULL,
					g_Cursors[hoveredIcon].fullPath,
					IMAGE_CURSOR,
					GetSystemMetrics(SM_CXCURSOR), GetSystemMetrics(SM_CYCURSOR),
					LR_LOADFROMFILE
				);
				g_hoveredCursor = hCursor;
			}
		}
	}
	break;
	case WM_SETCURSOR:
	{
		if (LOWORD(lParam) == HTCLIENT)
		{
			if (hoveredIcon >= 0 && g_hoveredCursor)
			{
				SetCursor(g_hoveredCursor);
				return TRUE;
			}
		}
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);;
	}
	break;
	case WM_NOTIFY:
	{
		LPNMHDR lpnmh = (LPNMHDR)lParam;

		if (lpnmh->hwndFrom == g_themesListView && lpnmh->code == LVN_KEYDOWN)
		{
			NMLVKEYDOWN* pKey = (NMLVKEYDOWN*)lParam;
			PostMessage(hWnd, WM_KEYDOWN, pKey->wVKey, 0);
			return TRUE;
		}
		else if (lpnmh->hwndFrom == g_themesListView && lpnmh->code == LVN_ITEMCHANGED) {
			LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lParam;
			if (lpnmlv->uChanged & LVIF_STATE) {
				if (lpnmlv->uNewState & LVIS_SELECTED) {
					SetFocus(g_themesListView);
					g_selectedTheme = g_themes.items[lpnmlv->iItem];
					hoveredIcon = -1;
					ShowWindow(g_applyButton, SW_NORMAL);
					ShowWindow(g_removeButton, SW_NORMAL);
					InvalidateRect(hWnd, NULL, TRUE);

					for (size_t i = 0; i < 17; i++)
					{
						if (g_selectedTheme.Icons[i][0] != '%')
							wsprintf(tmp, L"%s\\cursors\\%s", g_selectedTheme.BasePath, g_selectedTheme.Icons[i]);
						else
						{
							TCHAR expandedPath[MAX_PATH] = { 0 };

							ExpandEnvironmentStrings(
								g_selectedTheme.Icons[i],
								expandedPath,
								MAX_PATH
							);
							wcscpy(tmp, expandedPath);
						}

						HCURSOR hCursor = (HCURSOR)LoadImage(
							NULL,
							tmp,
							IMAGE_CURSOR,
							0, 0,
							LR_LOADFROMFILE
						);

						g_Cursors[i].hCursor = hCursor;
						wcscpy(g_Cursors[i].fullPath, tmp);
					}
				}
				else if ((lpnmlv->uOldState & LVIS_SELECTED) &&
					ListView_GetSelectedCount(g_themesListView) == 0) {
					g_selectedTheme = (Theme){ 0 };
					InvalidateRect(hWnd, NULL, TRUE);

					for (size_t i = 0; i < 17; i++)
						DestroyCursor(g_Cursors[i].hCursor);

					ShowWindow(g_applyButton, SW_HIDE);
					ShowWindow(g_removeButton, SW_HIDE);
				}

				POINT pt;
				GetCursorPos(&pt);
				ScreenToClient(hWnd, &pt);
				LPARAM lParam = MAKELPARAM(pt.x, pt.y);
				WPARAM wParam = 0;
				PostMessage(hWnd, WM_MOUSEMOVE, wParam, lParam);
				PostMessage(hWnd, WM_SETCURSOR, (WPARAM)hWnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
			}
		}
		break;
	}
	case WM_KEYDOWN:
	{
		int count = ListView_GetItemCount(g_themesListView);
		int focused = ListView_GetNextItem(g_themesListView, -1, LVNI_FOCUSED);

		switch (wParam)
		{
		case VK_F5:
			RefreshThemeList(hWnd, TRUE);
			break;
		case VK_ESCAPE:
			ListView_SetItemState(g_themesListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
			break;
		case VK_DOWN:
		{
			int next = (focused + 1) % count;
			ListView_SetItemState(g_themesListView, next,
				LVIS_FOCUSED | LVIS_SELECTED,
				LVIS_FOCUSED | LVIS_SELECTED);
			ListView_EnsureVisible(g_themesListView, next, FALSE);
		}
		break;
		case VK_UP:
		{
			if (focused == -1) focused = 0;
			int next = (focused - 1 + count) % count;
			ListView_SetItemState(g_themesListView, next,
				LVIS_FOCUSED | LVIS_SELECTED,
				LVIS_FOCUSED | LVIS_SELECTED);
			ListView_EnsureVisible(g_themesListView, next, FALSE);
		}
		break;
		case VK_RETURN:
		{
			if (g_selectedTheme.name)
				ApplyTheme(g_selectedTheme);
		}
		break;
		case VK_DELETE:
		{
			if (g_selectedTheme.name)
				RemoveSelectedTheme(hWnd);
		}
		break;
		default:
			break;
		}
		break;
	}
	case WM_CREATE:
		InitCommonControls();
		ThemeManager_Initialize(g_baseDir);
		{
			RECT size = { 0 };
			GetClientRect(hWnd, &size);
			g_themesListView = CreateWindowEx(
				WS_EX_CLIENTEDGE,
				WC_LISTVIEW,
				L"",
				WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
				0, 0, (size.right - size.left) / 2, size.bottom - size.top,
				hWnd,
				(HMENU)IDC_LISTVIEW,
				g_hInstance,
				NULL
			);

			ListView_SetExtendedListViewStyleEx(g_themesListView,
				LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES,
				LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER
			);

			LOGFONT lf = { 0 };
			HFONT hUiFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			GetObject(hUiFont, sizeof(LOGFONT), &lf);
			lf.lfHeight = 18;
			HFONT hFont = CreateFontIndirect(&lf);

			g_applyButton = CreateWindow(
				L"Button",
				L"Apply",
				WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
				0,
				0,
				0,
				0,
				hWnd,
				(HMENU)IDC_APPLYBUTTON,
				(HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
				NULL);

			SendMessage(g_applyButton, WM_SETFONT, (WPARAM)hFont, (LPARAM)0);

			g_removeButton = CreateWindow(
				L"Button",
				L"Delete",
				WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
				0,
				0,
				0,
				0,
				hWnd,
				(HMENU)IDC_REMOVE,
				(HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
				NULL);

			SendMessage(g_removeButton, WM_SETFONT, (WPARAM)hFont, (LPARAM)0);

			ShowWindow(g_applyButton, SW_HIDE);
			ShowWindow(g_removeButton, SW_HIDE);

			LVCOLUMN lvc = { 0 };
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.fmt = LVCFMT_LEFT;
			lvc.pszText = L"Available Themes";
			ListView_InsertColumn(g_themesListView, 0, &lvc);
			ListView_SetColumnWidth(g_themesListView, 0, LVSCW_AUTOSIZE_USEHEADER);

			HWND hHeader = ListView_GetHeader(g_themesListView);
			LONG_PTR style = GetWindowLongPtr(hHeader, GWL_STYLE);
			SetWindowLongPtr(hHeader, GWL_STYLE, style | HDS_NOSIZING | WS_DISABLED);

			SetWindowTheme(g_themesListView, L"Explorer", NULL);
		}
		RefreshThemeList(hWnd, FALSE);
		break;
	case WM_COMMAND: {
		int wmId = LOWORD(wParam);
		switch (wmId) {
		case IDC_REMOVE:
			RemoveSelectedTheme(hWnd);
			SetFocus(hWnd);
			break;
		case IDC_APPLYBUTTON:
			ApplyTheme(g_selectedTheme);
			SetFocus(hWnd);
			break;
		case ID_FILE_RESETTHEME:
			ApplyTheme(g_themes.items[0]);
			break;
		case ID_FILE_OPEN:
			ImportFiles(hWnd);
			break;

		case ID_FILE_OPENFOLDER:
			ImportFromFolder(hWnd);
			break;

		case ID_FILE_REFRESH:
			RefreshThemeList(hWnd, TRUE);
			break;

		case ID_FILE_EXIT:
			DestroyWindow(hWnd);
			break;

		case ID_HELP_ABOUT:
			ShowAbout(hWnd);
			break;
		}
		break;
	}
	case WM_SIZE:
	{
		int newWidth = LOWORD(lParam);
		int newHeight = HIWORD(lParam);

		int listViewWidth = newWidth / 4;
		SetWindowPos(g_themesListView, NULL, 0, 0, listViewWidth, newHeight, SWP_NOMOVE);

		SCROLLBARINFO sbi = { 0 };
		sbi.cbSize = sizeof(SCROLLBARINFO);
		BOOL scrollbarExists = GetScrollBarInfo(g_themesListView, OBJID_VSCROLL, &sbi);
		BOOL isEnabled = scrollbarExists && !(sbi.rgstate[0] & STATE_SYSTEM_INVISIBLE);
		ListView_SetColumnWidth(g_themesListView, 0,
			listViewWidth -
			GetSystemMetrics(SM_CXSIZEFRAME) -
			(isEnabled ? GetSystemMetrics(SM_CXVSCROLL) : 0));

		SIZE btnSize = { 0 };
		Button_GetIdealSize(g_applyButton, &btnSize);

		SetWindowPos(g_applyButton, NULL, newWidth - btnSize.cx, newHeight - btnSize.cy - 2, btnSize.cx, btnSize.cy, 0);

		Button_GetIdealSize(g_removeButton, &btnSize);

		SetWindowPos(g_removeButton, NULL, listViewWidth, newHeight - btnSize.cy - 2, btnSize.cx, btnSize.cy, 0);

		InvalidateRect(hWnd, NULL, TRUE);
	}
	break;
	case WM_DESTROY:
	case WM_CLOSE:
		for (size_t i = 1; i < g_themes.length; i++)
			ThemeManager_FreeTheme(&g_themes.items[i]);

		free(g_themes.items);
		ThemeManager_Cleanup();
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd) {

	GetModuleFileNameW(NULL, g_baseDir, MAX_PATH);
	PathRemoveFileSpecW(g_baseDir);

	g_hInstance = hInstance;

	const wchar_t CLASS_NAME[] = L"Cursory";

	WNDCLASSW wc = { 0 };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
	wc.lpszClassName = CLASS_NAME;
	wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MENU);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_APPLICATION));

	if (!RegisterClassW(&wc)) {
		ShowErrorMessage(NULL, L"Failed to register window class", L"Error");
		return 0;
	}

	HWND hwnd = CreateWindowExW(
		0,
		CLASS_NAME,
		L"Cursory",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hwnd == NULL) {
		ShowErrorMessage(NULL, L"Failed to create window", L"Error");
		return 0;
	}

	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}