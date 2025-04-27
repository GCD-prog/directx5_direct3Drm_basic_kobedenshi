#include <windows.h>
#include <stdio.h>
#include <math.h>

#include <ddraw.h>
//#include <d3drm.h>
#include <d3drmwin.h>
#include <mmsystem.h>	//マルチメディア系の制御（timeGetTime()で必要）

#pragma comment(lib, "ddraw.lib")
#pragma comment(lib, "d3drm.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")

#define PI			3.1415926535
#define SITA(c)		((double)(c)*PI/180.0)

#define NUM 10

#define INITGUID

#define WAIT 25

LPDIRECTDRAWSURFACE lpScreen;

LPDIRECTDRAW lpDirectDraw;
LPDIRECTDRAWCLIPPER lpDDClipper;
LPDIRECTDRAWSURFACE lpPrimary;
LPDIRECTDRAWSURFACE lpBackbuffer;
LPDIRECTDRAWSURFACE lpZbuffer;

LPDIRECT3D lpDirect3D;
LPDIRECT3DDEVICE lpD3DDevice;

LPDIRECT3DRM lpDirect3DRM;
LPDIRECT3DRMDEVICE lpD3DRMDevice;
LPDIRECT3DRMVIEWPORT lpD3DRMView;
LPDIRECT3DRMFRAME lpD3DRMScene;
LPDIRECT3DRMFRAME lpD3DRMCamera;

LPDIRECT3DRMFRAME lpObjFrame;

LPDIRECT3DRMFRAME lpLandFrame;

LPDIRECT3DRMFRAME lpTreeFrames[NUM];

//フレームを回転させる
void SetFrameRot(LPDIRECT3DRMFRAME lpFrame, int rx, int ry, int rz);
//フレームの位置指定
void SetFramePos(LPDIRECT3DRMFRAME lpFrame, double x, double y, double z);
//カメラの回転
void SetCameraRot(int rx, int ry, int rz);
//カメラの位置指定
void SetCameraPos(double x, double y, double z);

//シーンよりフレームを削除
void DeleteFrame(LPDIRECT3DRMFRAME lpFrame);

BOOL SetLight(void);

BOOL SetObject(void);

BOOL CreateTreeMesh(void);

BOOL AddLandscape(void);

// BOOL AddSkyMesh(void);

void ReleaseAll(void);

struct tagPLAYER{
	double x,y,z;		//自分の位置
	double view_x, view_y, view_z;	// 視点の方位角: view_x , 視点の仰角: view_y , 視点と注視点の距離: view_z
	int rx,ry,rz;		//自分のX,Y回転角
} Player;

char szDevice[128], szDDDeviceName[128] = "default";

double fx=0.0,fz=0.0;
int ry=0;

void FrameCnt(void)
{
	     static int cnt;
		 static DWORD Nowtime,Prevtime;
		 static char text0[10];
 		 static char text1[50], text2[50], text3[50], text4[50], text5[50], text6[50], text7[50];
		 static char Data[256];

		 HDC hdc;
		 cnt++;
		 Nowtime = timeGetTime();
		 if((Nowtime - Prevtime) >= 1000){
			 Prevtime = Nowtime;
			 wsprintf(text0,"%d fps", cnt);
			 cnt = 0;
		 }

		 sprintf(Data, "%s , [%s]", szDevice, szDDDeviceName);

		 // 開発時のみ表示する
		 lpBackbuffer->GetDC(&hdc);
		 TextOut(hdc, 0, 0, text0, strlen(text0));

		 TextOut(hdc, 0, 20, Data, strlen(Data));
		 lpBackbuffer->ReleaseDC(hdc);
}

void KeyControl(void)
{
	//処理を書き込む

	ry = (Player.ry -90 + 360) % 360;	//Player.ryが0なら正面を向いている

	fx = cos(SITA(ry));
	fz = -sin(SITA(ry));

	//上
	if(GetAsyncKeyState('Z')&0x8000){
		Player.y += 2;
	}

	//下
	if(GetAsyncKeyState('X')&0x8000){
		Player.y -= 2;
	}

	//視点を上に傾ける
	if(GetAsyncKeyState(VK_UP)&0x8000){
		if(Player.rx > -30) Player.rx -= 2;
	}

	//視点を下に傾ける
	if(GetAsyncKeyState(VK_DOWN)&0x8000){		
		if(Player.rx < 30) Player.rx += 2;
	}

	//左旋回
	if( (GetAsyncKeyState(VK_LEFT)&0x8000) || (GetAsyncKeyState('Q')&0x8000) ){
		Player.rz -= 2;
		Player.ry -= 2;
	}

	//右旋回
	if( (GetAsyncKeyState(VK_RIGHT)&0x8000) || (GetAsyncKeyState('E')&0x8000) ){
		Player.rz += 2;
		Player.ry += 2;
	}

	//前進
	if(GetAsyncKeyState('W')&0x8000){
		Player.x += fx;
		Player.z += fz;
	}

	//後退
	if(GetAsyncKeyState('S')&0x8000){
		Player.x -= fx;
		Player.z -= fz;
	}

	//左
	if(GetAsyncKeyState('A')&0x8000){
		Player.x -= fz;
	}

	//右
	if(GetAsyncKeyState('D')&0x8000){
		Player.x += fz;
	}

	//視点の位置と方向を更新
	Player.ry = (Player.ry + 360) % 360;		//この値は0-359の値

}

BOOL RenderFrame(void)
{

	if ( lpPrimary->IsLost() == DDERR_SURFACELOST )		lpPrimary->Restore();

		RECT Scrrc={0, 0, 640, 480};   //画面のサイズ

		//秒間６０フレームを越えないようにする
		static DWORD nowTime, prevTime;
		nowTime = timeGetTime();
		if( (nowTime - prevTime) < 1000 / 60 ) return 0;
		prevTime = nowTime;

		//キー入力
		KeyControl();

		// --- オブジェクト更新 ---
	    // 位置更新 (シーン基準)
		SetFramePos(lpD3DRMCamera, Player.x, Player.y, Player.z);

		//カメラフレームの移動 yawはY軸、pitchはX軸、rollはZ軸の回転
		SetCameraPos(Player.x, Player.y, Player.z);
		SetCameraRot((Player.rx + 360) % 360, Player.ry, 0);		//FPS視点の回転

		//木の作成
		int i;
		for(i = 0; i < NUM; i++){
			//木を視点の方向に向かせる , ビルボード処理
			ry = (Player.ry - 180 + 360) % 360;
			SetFrameRot(lpTreeFrames[i], 0, ry, 0);

/*
			if (lpTreeFrames[i]) {					//lpTreeFrames[i] が有効な場合のみ削除
				DeleteFrame(lpTreeFrames[i]);		//既存のオブジェクトフレームをシーンから削除し、解放
				lpTreeFrames[i] = NULL;				//削除したのでポインタをNULLにする
			}
*/

		}

		// --- 地面の再配置 このロジックが、プレイヤーの位置に合わせて地面を再作成することで
		// 地面が手前に動いているように見せる効果を生み出します。
		if (lpLandFrame) {				//lpLandFrame が有効な場合のみ削除
			DeleteFrame(lpLandFrame);	//既存の地面フレームをシーンから削除し、解放
			lpLandFrame = NULL;			//削除したのでポインタをNULLにする
		}
		AddLandscape();					//新しい地面フレームをプレイヤーの現在位置を基準に作成

		//Direct3DRM レンダリング処理
		lpD3DRMScene->Move(D3DVAL(1.0)); 
		lpD3DRMView->Clear();
		
		// 2Dスプライトを表示する場合ここに記述
		//背景をバックバッファに転送
		// lpBackbuffer->BltFast(0, 0, lpScreen, &Scrrc, DDBLTFAST_NOCOLORKEY | DDBLTFAST_WAIT);

		lpD3DRMView->Render(lpD3DRMScene);
		lpD3DRMDevice->Update();

		//FPS値計測
		FrameCnt();

		// Flip関数は使わない 画面がちらつく
		// lpPrimary->Flip(NULL, DDFLIP_WAIT);

		//  Flip関数を使わないフリップ処理
		RECT rc;
		SetRect(&rc, 0, 0, 640, 480);  //  画面全体を指定
		lpPrimary->BltFast(0, 0, lpBackbuffer, NULL, DDBLTFAST_NOCOLORKEY);

		return TRUE;
}

//-------------------------------------------
// ドライバ検索処理関数
//-------------------------------------------
GUID* D3D_GuidSearch(HWND hwnd)
{
	HRESULT d3dret;   //ダイレクト３Ｄ系関数の戻り値を格納する変数
	GUID*   Guid;
	LPDIRECT3D          lpD3D;
	LPDIRECTDRAW        lpDD;
	D3DFINDDEVICESEARCH S_DATA;
	D3DFINDDEVICERESULT R_DATA;
	char str[100];

	//GUID取得の準備
	memset(&S_DATA, 0, sizeof S_DATA);
	S_DATA.dwSize = sizeof S_DATA;
	S_DATA.dwFlags = D3DFDS_COLORMODEL;
	S_DATA.dcmColorModel = D3DCOLOR_RGB;
	memset(&R_DATA, 0, sizeof R_DATA);
	R_DATA.dwSize = sizeof R_DATA;

	//DIRECTDRAWの生成
	d3dret = DirectDrawCreate(NULL, &lpDD, NULL);
	if (d3dret != DD_OK) {
		MessageBox( hwnd, "ダイレクトドローオブジェクトの生成に失敗しました", "初期化", MB_OK);
		lpDD->Release();
		return NULL;
	}

	//DIRECTD3Dの生成
	d3dret = lpDD->QueryInterface(IID_IDirect3D, (void**)&lpD3D);
	if (d3dret != D3D_OK) {
		MessageBox( hwnd, "ダイレクト３Ｄオブジェクトの生成に失敗しました", "初期化", MB_OK);
		lpDD->Release();
		lpD3D->Release();
		return NULL;
	}
	//デバイスの列挙
	d3dret = lpD3D->FindDevice(&S_DATA, &R_DATA);
	if (d3dret != D3D_OK) {
		MessageBox( hwnd, "デバイスの列挙に失敗しました", "初期化", MB_OK);
		lpDD->Release();
		lpD3D->Release();
		return NULL;
	}

	//ガイドの取得
	Guid = &R_DATA.guid;
	//不要になったオブジェクトのリリース
	lpDD->Release();
	lpD3D->Release();
	//OutputDebugString(str);
	wsprintf(str, "%x", *Guid);
	return (Guid);
}

/*-------------------------------------------
// DirectDraw デバイスの列挙と選定
--------------------------------------------*/
BOOL CALLBACK DDEnumCallback(GUID FAR* lpGUID, LPSTR lpDriverDesc, LPSTR lpDriverName, LPVOID lpContext)
{
	LPDIRECTDRAW lpDD;
	DDCAPS DriverCaps, HELCaps;

	// DirectDraw オブジェクトを試験的に生成する
	if(DirectDrawCreate(lpGUID, &lpDD, NULL) != DD_OK) {
		*(LPDIRECTDRAW*)lpContext = NULL;
		return DDENUMRET_OK;
	}

	// DirectDrawの能力を取得
	ZeroMemory(&DriverCaps, sizeof(DDCAPS));
	DriverCaps.dwSize = sizeof(DDCAPS);
	ZeroMemory(&HELCaps, sizeof(DDCAPS));
	HELCaps.dwSize = sizeof(DDCAPS);

	if(lpDD->GetCaps(&DriverCaps, &HELCaps) == DD_OK) {
	// ハードウェア3D支援が期待できる場合で．
	// なおかつテクスチャが使用できる場合それを使う
		if ((DriverCaps.dwCaps & DDCAPS_3D) && (DriverCaps.ddsCaps.dwCaps & DDSCAPS_TEXTURE)) {
			*(LPDIRECTDRAW*)lpContext = lpDD;
			lstrcpy(szDDDeviceName, lpDriverDesc);
			return DDENUMRET_CANCEL;
		}
	}

	// 他のドライバを試す
	*(LPDIRECTDRAW*)lpContext = NULL;
	lpDD->Release();

	return DDENUMRET_OK;
}

LRESULT APIENTRY WndFunc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

	switch(msg){

	case WM_CREATE:

		break;

	case WM_KEYDOWN:

				// Escキーでプログラムを終了します
		if(wParam == VK_ESCAPE){

			//画面モードを元に戻す
			lpDirectDraw->SetCooperativeLevel(hwnd, DDSCL_NORMAL);
			lpDirectDraw->RestoreDisplayMode();

			lpScreen->Release();

			ReleaseAll(); //各オブジェクトをReleaseAll()で解放する

			PostQuitMessage(0);

		}

		break;

	case WM_DESTROY:

			lpScreen->Release();

			ReleaseAll(); //各オブジェクトをReleaseAll()で解放する

			PostQuitMessage(0);

		break;

	default:
		return (DefWindowProc(hwnd, msg, wParam, lParam));
	}
	return 0;
}

void LoadBMP(LPDIRECTDRAWSURFACE lpSurface, char *fname)
{
	     HBITMAP hBmp = NULL;
		 BITMAP bm;
		 HDC hdc, hMemdc;

		 hBmp = (HBITMAP)LoadImage(GetModuleHandle(NULL), fname, IMAGE_BITMAP, 0, 0,
			      LR_CREATEDIBSECTION | LR_LOADFROMFILE);
		 GetObject(hBmp, sizeof(bm), &bm);

		 hMemdc = CreateCompatibleDC(NULL);
		 SelectObject(hMemdc, hBmp);

		 lpSurface->GetDC(&hdc);
		 BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, hMemdc, 0, 0, SRCCOPY);
		 lpSurface->ReleaseDC(hdc);

		 DeleteDC(hMemdc);
		 DeleteObject(hBmp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpszCmdParam, int nCmdShow)
{
		lpDirectDraw = NULL;
		lpDDClipper = NULL;
		lpPrimary = NULL;
		lpBackbuffer = NULL;
		lpZbuffer = NULL;
		lpDirect3DRM = NULL;
		lpD3DRMDevice = NULL;
		lpD3DRMView = NULL;
		lpD3DRMScene = NULL;

		MSG msg;
		HWND hwnd;

		DDSURFACEDESC Dds;
		DDSCAPS Ddscaps;

		WNDCLASS wc;
		char szAppName[] = "Generic Game SDK Window";
		
		wc.style = CS_DBLCLKS;
		wc.lpfnWndProc = WndFunc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hInst;
		wc.hIcon = LoadIcon(NULL,IDI_APPLICATION);
		wc.hCursor = LoadCursor(NULL,IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = szAppName;

		RegisterClass(&wc);

		hwnd=CreateWindowEx(
							WS_EX_TOPMOST,
							szAppName,
							"Direct X",
							WS_VISIBLE | WS_POPUP,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							640, 480,
							NULL, NULL, hInst,
							NULL);

		if(!hwnd) return FALSE;

		ShowWindow(hwnd, nCmdShow);
		UpdateWindow(hwnd);
		SetFocus(hwnd);

		ShowCursor(FALSE); //カーソルを隠す

		//Direct3DRMの構築
		Direct3DRMCreate(&lpDirect3DRM);

		//DirectDrawClipperの構築
		DirectDrawCreateClipper(0, &lpDDClipper, NULL);

		// DirectDrawドライバを列挙する
		DirectDrawEnumerate(DDEnumCallback, &lpDirectDraw);

		// 列挙によってDirectDrawドライバを決める
		// 決定しなかった場合、現在アクティブなドライバを使う
		if(!lpDirectDraw){
			lstrcpy(szDDDeviceName, "Active Driver");
			DirectDrawCreate(NULL, &lpDirectDraw, NULL);
		}

		lpDirectDraw->SetCooperativeLevel(hwnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT);

		//ディスプレイモード変更
		lpDirectDraw->SetDisplayMode(640, 480, 16);

		//基本サーフェスとフロントバッファの生成 (１つを作成)
		ZeroMemory(&Dds, sizeof(DDSURFACEDESC));
		Dds.dwSize = sizeof(DDSURFACEDESC);
		Dds.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		Dds.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY | DDSCAPS_3DDEVICE;
		Dds.dwBackBufferCount = 1;

		lpDirectDraw->CreateSurface(&Dds, &lpPrimary, NULL);

		//バックバッファのポインタ取得
		Ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
		lpPrimary->GetAttachedSurface(&Ddscaps, &lpBackbuffer);

		// Z-Buffer作成
		//基本サーフェスとバッファ１つを作成
		ZeroMemory(&Dds, sizeof(DDSURFACEDESC));
		Dds.dwSize = sizeof(DDSURFACEDESC);
		Dds.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_ZBUFFERBITDEPTH;
		Dds.dwHeight = 640;
		Dds.dwWidth = 480;
		Dds.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | DDSCAPS_VIDEOMEMORY;
		Dds.dwZBufferBitDepth = 16;

		lpDirectDraw->CreateSurface(&Dds, &lpZbuffer, NULL);

		lpBackbuffer->AddAttachedSurface(lpZbuffer);

		//DirectDrawでのアクセスもできるように、Clipperをつける
		struct _MYRGNDATA {
			RGNDATAHEADER rdh;
			RECT rc;
		}rgndata;
	
		rgndata.rdh.dwSize = sizeof(RGNDATAHEADER);
		rgndata.rdh.iType = RDH_RECTANGLES;
		rgndata.rdh.nCount = 1;
		rgndata.rdh.nRgnSize = sizeof(RECT)*1;
		rgndata.rdh.rcBound.left = 0;
		rgndata.rdh.rcBound.right = 640;
		rgndata.rdh.rcBound.top = 0;
		rgndata.rdh.rcBound.bottom = 480;

		rgndata.rc.top = 0;
		rgndata.rc.bottom = 480;
		rgndata.rc.left = 0;
		rgndata.rc.right = 640;

		lpDirectDraw->CreateClipper(0, &lpDDClipper, NULL);
		lpDDClipper->SetClipList((LPRGNDATA)&rgndata, NULL);
		lpBackbuffer->SetClipper(lpDDClipper);

		DDCOLORKEY          ddck;

		//背景サーフェスを作成
		Dds.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		Dds.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		Dds.dwWidth = 640;
		Dds.dwHeight = 480;
		lpDirectDraw->CreateSurface(&Dds, &lpScreen, NULL);

		//カラーキーの指定 白:RGB(255, 255, 255)　黒:RGB(0, 0, 0)
		ddck.dwColorSpaceLowValue=RGB(0, 0, 0);
		ddck.dwColorSpaceHighValue=RGB(0, 0, 0);
		lpScreen->SetColorKey(DDCKEY_SRCBLT, &ddck);

		//各サーフェスに画像を読み込む
		LoadBMP(lpScreen, "datafile\\back.BMP");  //背景

		DirectDrawCreateClipper(0, &lpDDClipper, NULL);

//Direct3DRMの初期化 ここから

//デバイスの生成 ＨＥＬ実行 RAMP 低画質 速度遅い
//		Direct3DRMCreate(&lpDirect3DRM);
//		lpDirect3DRM->CreateDeviceFromSurface(NULL, lpDirectDraw, lpBackbuffer, &lpD3DRMDevice);
//

//デバイスの生成 ＨＡＬで実行する

		HRESULT ddret;   //ダイレクト３Ｄ系関数の戻り値を格納する変数
		GUID*   Guid;

		Direct3DRMCreate(&lpDirect3DRM);

		Guid = D3D_GuidSearch(hwnd);
		// HAL
		ddret = lpDirect3DRM->CreateDeviceFromSurface(Guid, (IDirectDraw*)lpDirectDraw, lpBackbuffer, &lpD3DRMDevice);
			strcpy(szDevice,"D3D HAL");
		if (ddret != D3DRM_OK) {
			MessageBox( hwnd, "デバイスの生成に失敗、ＨＡＬでの実行は不可能です", "", MB_OK);
			//HALでの実行が不可能な時、HELでの実行を行う
			ddret = lpDirect3DRM->CreateDeviceFromSurface(Guid, (IDirectDraw*)lpDirectDraw, lpBackbuffer, &lpD3DRMDevice);
			if (ddret != D3DRM_OK) {
				strcpy(szDevice,"HEL");
				MessageBox( hwnd, "ＨＥＬでの、デバイスの生成に失敗、Direct3Dの使用は不可能です", "", MB_OK);
			}

			if(ddret != D3DRM_OK){
				//MMX
				ddret = lpDirect3DRM->CreateDeviceFromSurface(Guid, (IDirectDraw*)lpDirectDraw, lpBackbuffer, &lpD3DRMDevice);
				strcpy(szDevice,"D3D MMX Emulation");
			}

			if(ddret != D3DRM_OK){
				//RGB
				ddret = lpDirect3DRM->CreateDeviceFromSurface(Guid, (IDirectDraw*)lpDirectDraw, lpBackbuffer, &lpD3DRMDevice);
				strcpy(szDevice,"D3D RGB Emulation");
			}
		}

		lpD3DRMDevice->SetQuality(D3DRMLIGHT_ON | D3DRMFILL_SOLID | D3DRMSHADE_GOURAUD);

		lpDirect3DRM->CreateFrame(NULL, &lpD3DRMScene);

		//カメラを作成
								// 親フレーム ,	子フレーム
		lpDirect3DRM->CreateFrame(lpD3DRMScene, &lpD3DRMCamera);
	
		//カメラの位置を設定
		Player.x = 0.0;
		Player.y = 3.0;
		Player.z = 0.0;
		Player.ry = 0;

		SetCameraPos(Player.x, Player.y, Player.z);
		SetCameraRot(0, Player.ry, 0);

		//デバイスとカメラからDirect3DRMViewPortを作成
		lpDirect3DRM->CreateViewport(lpD3DRMDevice, lpD3DRMCamera, 0, 0, 640, 480, &lpD3DRMView);
		lpD3DRMView->SetBack(D3DVAL(5000.0));

	//ゲームに関する初期化処理（開始）---------------------------

		SetLight();

		SetObject();

		CreateTreeMesh();

		AddLandscape();

		// AddSkyMesh(void)

	//ゲームに関する初期化処理（終了）---------------------------

		while(1){

			if(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
				{
					if(!GetMessage(&msg, NULL, 0, 0))
						return msg.wParam;
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}else{

							RenderFrame();

					}
		}
		return msg.wParam;

		//ゲームに関する終了処理（開始）---------------------------


		//ゲームに関する終了処理（終了）---------------------------
}

//フレームの位置移動
void SetFramePos(LPDIRECT3DRMFRAME lpFrame, double x, double y, double z)
{
	lpFrame->SetPosition(lpD3DRMScene, D3DVAL(x), D3DVAL(y), D3DVAL(z));
	lpFrame->Release();
}

//フレームを回転させる
void SetFrameRot(LPDIRECT3DRMFRAME lpFrame,int rx,int ry,int rz)
{
	double sinx, siny, sinz, cosx, cosy, cosz;
	double y1, y2, y3, z1, z2, z3;
	double d1, d2;

	if(lpFrame==NULL) return;

	y1 = SITA(rx);
	y2 = SITA(ry);
	y3 = SITA(rz);

	sinx = sin(y1);		cosx = cos(y1);
	siny = sin(y2);		cosy = cos(y2);
	sinz = sin(y3);		cosz = cos(y3);
	
	d1 = sinx * siny;
	d2 = cosx * siny;
	//Ｙ軸の回転後のベクトル
	y1 = d1 * cosz - cosx * sinz;
	y2 = d1 * sinz + cosx * cosz;
	y3 = sinx * cosy;
	//Ｚ軸の回転後のベクトル
	z1 = d2 * cosz + sinx * sinz;
	z2 = d2 * sinz - sinx * cosz;
	z3 = cosx * cosy;

	lpFrame->SetOrientation(lpD3DRMScene, D3DVAL(z1), D3DVAL(z2), D3DVAL(z3), D3DVAL(y1), D3DVAL(y2), D3DVAL(y3));

	lpFrame->Release();
}

//カメラの回転
void SetCameraRot(int rx, int ry, int rz){
	SetFrameRot(lpD3DRMCamera, rx, ry, rz);
}

//カメラの位置指定
void SetCameraPos(double x, double y, double z){	
	SetFramePos(lpD3DRMCamera, x, y, z);
}

/*
//座標の変換（ワールド座標－＞スクリーン座標へ）
//(wx,wy,wz) ... ワールド座標上の１点
//(rx,ry)  ... スクリーン座標に変換せれた座標を格納する
BOOL Transform(double wx,double wy,double wz,int *rx,int *ry)
{
	D3DVECTOR SrcV;
	D3DRMVECTOR4D DstV;
	int x, y;
	
	SrcV.x = D3DVAL(wx);
	SrcV.y = D3DVAL(wy);
	SrcV.z = D3DVAL(wz);
	//ワールド座標－＞スクリーン座標に変換
	lpD3DRMView->Transform(&DstV, &SrcV);
	if(DstV.z > 0 && DstV.w > 0){
		x = (int)(DstV.x / DstV.w);
		y = (int)(DstV.y / DstV.w);
		if(x > 0 && x < 640 && y > 0 && y < 480){
			*rx = x;
			*ry = y;
			return TRUE;
		}
	}
	return FALSE;
}
*/

//シーンよりフレームを削除
void DeleteFrame(LPDIRECT3DRMFRAME lpFrame)
{
	if(lpFrame == NULL) return;
	lpD3DRMScene->DeleteChild(lpFrame);

}

//光源の設定
BOOL SetLight(void)
{

	//アンビエント光源を配置
	LPDIRECT3DRMLIGHT lpD3DRMLightAmbient;
	
	lpDirect3DRM->CreateLightRGB(D3DRMLIGHT_AMBIENT, D3DVAL(5.0), D3DVAL(5.0), D3DVAL(5.0), &lpD3DRMLightAmbient);
	lpD3DRMScene->AddLight(lpD3DRMLightAmbient);
	lpD3DRMLightAmbient->Release();

	LPDIRECT3DRMFRAME lpD3DRMLightFrame;
	lpDirect3DRM->CreateFrame(lpD3DRMScene, &lpD3DRMLightFrame);
	
	//ポイント光源を配置
	LPDIRECT3DRMLIGHT lpD3DRMLightPoint;

	lpDirect3DRM->CreateLightRGB(D3DRMLIGHT_POINT, D3DVAL(0.9), D3DVAL(0.9), D3DVAL(0.9), &lpD3DRMLightPoint);
	
	lpD3DRMLightFrame->SetPosition(lpD3DRMScene, D3DVAL(10.0), D3DVAL(0.0), D3DVAL(0.0));
	lpD3DRMLightFrame->AddLight(lpD3DRMLightPoint);

	lpD3DRMLightPoint->Release();

	lpD3DRMLightFrame->Release();

	return TRUE;

}

//オブジェクトのロード（Xファイルの読み込み）Sennsya.x
BOOL SetObject(void)
{

	LPDIRECT3DRMTEXTURE lpTex = NULL;

	LPDIRECT3DRMWRAP lpWrap = NULL;

	//Direct3DRMMeshBuilderを作成
	LPDIRECT3DRMMESHBUILDER lpObj_mesh = NULL;

	//メッシュが存在すれば削除
	if(lpObj_mesh != NULL){
		lpObjFrame->DeleteVisual(lpObj_mesh);		//メッシュをフレームから消す
		DeleteFrame(lpObjFrame);					//フレームをシーンから削除する
		lpObjFrame->Release();
	}

	lpDirect3DRM->CreateMeshBuilder(&lpObj_mesh);

	// Sennsya.x を読み込む
	lpObj_mesh->Load("datafile\\Sennsya.x", NULL, D3DRMLOAD_FROMFILE, NULL, NULL);
	lpObj_mesh->Scale(D3DVAL(0.8), D3DVAL(0.8), D3DVAL(0.8));
	lpObj_mesh->SetColorRGB(D3DVAL(1), D3DVAL(1), D3DVAL(1));

	D3DVALUE miny, maxy, height;
	D3DRMBOX box;

	lpObj_mesh->GetBox(&box);

	maxy = box.max.y;
	miny = box.min.y;
	height = maxy - miny;

	lpDirect3DRM->CreateWrap(D3DRMWRAP_CYLINDER, NULL,
							 D3DVAL(0.0), D3DVAL(0.0), D3DVAL(0.0),
						     D3DVAL(0.0), D3DVAL(1.0), D3DVAL(0.0),
						     D3DVAL(0.0), D3DVAL(0.0), D3DVAL(1.0),
							 D3DVAL(0.0), D3DDivide(miny, height),
							 D3DVAL(1.0), D3DDivide(-D3DVAL(1.0), height), &lpWrap);
	lpWrap->Apply(lpObj_mesh);

	//Xファイルにテクスチャを張り付け
	lpDirect3DRM->LoadTexture("datafile\\hello.bmp", &lpTex);

	lpTex->SetShades(16);

	lpObj_mesh->SetTexture(lpTex);

	lpDirect3DRM->CreateFrame(lpD3DRMCamera, &lpObjFrame);

	// SetPosition() 参照フレームを基準とするフレームの位置を設定
	lpObjFrame->SetPosition(lpD3DRMScene, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(10.0));

	lpObjFrame->SetOrientation(lpD3DRMScene, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(1.0), D3DVAL(0.0), D3DVAL(1.0), D3DVAL(0.0));

	lpObjFrame->AddVisual((LPDIRECT3DRMVISUAL)lpObj_mesh);

	lpObjFrame->Release();

	lpObj_mesh->Release();

	lpWrap->Release();

	lpTex->Release();

	return TRUE;

}

//地面のメッシュを作成する
//１ポリゴンで表現 
BOOL AddLandscape(void)
{
	int ry;
	double x,z;
	double ex,ey,ez;
	double stx,stz;
	double scale;

	D3DVECTOR face_ver[4];
	LPDIRECT3DRMFACEARRAY lpFaceA;
	LPDIRECT3DRMFACE lpFace;
	
	ex=Player.x;
	ey=Player.y;
	ez=Player.z;
	
	LPDIRECT3DRMTEXTURE lpLandtex=NULL;
    
	lpDirect3DRM->LoadTexture("datafile\\Texture.bmp", &lpLandtex);


	//頂点の１つ目は視点より少し後方のところ
	ry=Player.ry-90;
	ry=(ry+360)%360;
	x=ex-(10.0*cos(SITA(ry)));
	z=ez-(-10.0*sin(SITA(ry)));
	face_ver[0].x=D3DVAL(x);face_ver[0].y=D3DVAL(0);face_ver[0].z=D3DVAL(z);

	//頂点２つ目
	ry=Player.ry-90-45;
	ry=(ry+360)%360;
	x=(int)(200.0*cos(SITA(ry)))+ex;
	z=(int)(-200.0*sin(SITA(ry)))+ez;
	face_ver[1].x=D3DVAL(x);face_ver[1].y=D3DVAL(0);face_ver[1].z=D3DVAL(z);

	//頂点３つ目
	ry=Player.ry-90+45;
	ry=(ry+360)%360;
	x=(int)(200.0*cos(SITA(ry)))+ex;
	z=(int)(-200.0*sin(SITA(ry)))+ez;
	face_ver[2].x=D3DVAL(x);face_ver[2].y=D3DVAL(0);face_ver[2].z=D3DVAL(z);

	//結合状態設定
	DWORD face_data[]={3,0,0,1,1,2,2,0};

	//各頂点ごとの法線の設定
	D3DVECTOR face_normal[]={
		D3DVAL(0),D3DVAL(1),D3DVAL(0),
		D3DVAL(0),D3DVAL(1),D3DVAL(0),
		D3DVAL(0),D3DVAL(1),D3DVAL(0),
	};

	//メッシュの生成
	LPDIRECT3DRMMESHBUILDER lpLand_mesh = NULL;

	//地面のメッシュが存在すれば削除
	if(lpLand_mesh!=NULL){
		lpLandFrame->DeleteVisual(lpLand_mesh);		//メッシュをフレームから消す
		DeleteFrame(lpLandFrame);					//フレームをシーンから削除する
		lpLandFrame->Release();
	}

	//メッシュの生成
	lpDirect3DRM->CreateMeshBuilder(&lpLand_mesh);
	lpLand_mesh->AddFaces(3,face_ver,3,face_normal,face_data,&lpFaceA);
	lpLand_mesh->SetPerspective(TRUE);

	//テクスチャをポリゴンに張り付ける
	//scaleを小さくすることでテクスチャはより細かくなる
	scale=20.0;
	lpFaceA->GetElement(0,&lpFace);
	lpFace->SetColor(D3DRGB(1.0,1.0,1.0));
	stx=(face_ver[0].x)/scale;stz=(face_ver[0].z)/scale;
	lpFace->SetTextureCoordinates(0,D3DVAL(stx),D3DVAL(stz));
	stx=(face_ver[1].x)/scale;stz=(face_ver[1].z)/scale;
	lpFace->SetTextureCoordinates(1,D3DVAL(stx),D3DVAL(stz));
	stx=(face_ver[2].x)/scale;stz=(face_ver[2].z)/scale;
	lpFace->SetTextureCoordinates(2,D3DVAL(stx),D3DVAL(stz));
	lpFace->SetTexture(lpLandtex);
	lpFace->Release();
	lpFaceA->Release();

	//フレームを作成し、地面のメッシュを結びつける
	lpDirect3DRM->CreateFrame(lpD3DRMScene, &lpLandFrame);

	lpLandFrame->AddVisual(lpLand_mesh);

	lpLandFrame->Release();

    lpLand_mesh->Release();

	lpLandtex->Release();

	return TRUE;
}

//木のメッシュを作成する　1ポリゴン（厳密には３角形ポリゴン２つ）で造る
BOOL CreateTreeMesh(void)
{
	int i;
	int x, y, z;

	LPDIRECT3DRMTEXTURE lpTreeTex;		//木のテクスチャ用

	lpDirect3DRM->LoadTexture("datafile\\tree.bmp", &lpTreeTex);

	//テクスチャの透過色（青色を透明とする）の設定
	lpTreeTex->SetDecalTransparentColor( D3DRGB( D3DVAL(0.0), D3DVAL(0.0), D3DVAL(1.0) ));
	lpTreeTex->SetDecalTransparency(TRUE);

	//////////////////////////////////
	//	木のメッシュ作成			//
	//////////////////////////////////
	LPDIRECT3DRMFACEARRAY lpFaceA;
	LPDIRECT3DRMFACE lpFace;

	//木の頂点座標設定
	D3DVECTOR face_ver[] = {
		D3DVAL(10), D3DVAL(16), D3DVAL(0),
		D3DVAL(-10), D3DVAL(16), D3DVAL(0),
		D3DVAL(-10), D3DVAL(0), D3DVAL(0),
		D3DVAL(10), D3DVAL(0), D3DVAL(0)
	};
	//結合状態設定
	DWORD face_data[] = {4,0,0,1,1,2,2,3,3,0};
	//各頂点ごとの法線の設定
	D3DVECTOR face_normal[] = {
		D3DVAL(0), D3DVAL(1), D3DVAL(0),
		D3DVAL(0), D3DVAL(1), D3DVAL(0),
		D3DVAL(0), D3DVAL(1), D3DVAL(0),
		D3DVAL(0), D3DVAL(1), D3DVAL(0)
	};

	LPDIRECT3DRMMESHBUILDER lpTree_mesh;	//木の形状用

	//メッシュ生成
	lpDirect3DRM->CreateMeshBuilder(&lpTree_mesh);
	lpTree_mesh->AddFaces(4, face_ver, 4, face_normal, face_data, &lpFaceA);
	//テクスチャのゆがみがでないようにする
	lpTree_mesh->SetPerspective(TRUE);

	//テクスチャをポリゴンに張り付ける
	lpFaceA->GetElement(0,&lpFace);
	lpFace->SetColor(D3DRGB(1.0, 1.0, 1.0));
	lpFace->SetTextureCoordinates(0, D3DVAL(0), D3DVAL(1.0));
	lpFace->SetTextureCoordinates(1, D3DVAL(1.0), D3DVAL(1.0));
	lpFace->SetTextureCoordinates(2, D3DVAL(1.0), D3DVAL(0));
	lpFace->SetTextureCoordinates(3, D3DVAL(0), D3DVAL(0));
	lpFace->SetTexture(lpTreeTex);
	lpFace->Release();

	//オブジェ削除
	for(i=0; i < NUM; i++){
		if(lpTreeFrames[i] != NULL){
			lpTreeFrames[i]->DeleteVisual(lpTree_mesh);	//メッシュをフレームから消す
			DeleteFrame(lpTreeFrames[i]);	//フレームをシーンから削除する
			lpTreeFrames[i]->Release();
		}
	}

	//木を配置する
	for(i=0; i < NUM; i++){
		//フレームを作成し、木のメッシュを結びつける
								// 親フレーム ,	子フレーム
		lpDirect3DRM->CreateFrame(lpD3DRMScene, &lpTreeFrames[i]);

		//メッシュをフレームに結びつける
		lpTreeFrames[i]->AddVisual(lpTree_mesh);

		//フレームの位置を設定
		x = rand() % 60 - 30;
		y = 0;
		z = rand() % 60 + 5;
		SetFramePos(lpTreeFrames[i], x, y, z);
		SetFrameRot(lpTreeFrames[i], 0, 0, 0);

		lpTreeFrames[i]->Release();
	    lpTree_mesh->Release();
	}

	lpTreeTex->Release();

	return TRUE;
}

/*
//空の天球のメッシュを生成する
//37頂点(1+4*8+4)、32ポリゴン(4*8)の半球を造る
BOOL AddSkyMesh(void)
{

	LPDIRECT3DRMTEXTURE lpSkytex = NULL;    

	lpDirect3DRM->LoadTexture("datafile\\cloud.bmp", &lpSkytex);

	int n1,n2,n3;
	int i,j,s,s2;
	double R=320.0;		//球の半径
	double x,y,z;
	D3DRMVERTEX Ver[37];
	D3DRMVERTEX *lpVer;
	D3DVECTOR *lpVec;
	D3DVECTOR *lpNor;
	unsigned int face_data[32*5];
	unsigned int *lpPos;
	D3DRMGROUPINDEX id;

	///////////////////////////////////////
	//天球の頂点を作成する
	lpVer=Ver;
	lpVec=&(lpVer->position);
	lpNor=&(lpVer->normal);
	//てっぺんの座標を0とする
	lpVec->x=D3DVAL(0);lpVec->y=D3DVAL(R);lpVec->z=D3DVAL(0);
	lpNor->x=D3DVAL(0),lpNor->y=D3DVAL(-1);lpNor->z=D3DVAL(0);
	lpVer->tu=D3DVAL(0);lpVer->tv=D3DVAL(1);
	lpVer->color=D3DRGB(1,1,1);
	lpVer++;
	for(i=1;i<37;i++){
		lpVec=&(lpVer->position);
		lpNor=&(lpVer->normal);
		n1=((i-1)&3)+1;
		n2=(i-1)>>2;
		n3=n2*45;
		//頂点座標の計算
		x=((double)n1*R*(cos(SITA(n3))))/4.0;
		y=(1.0-(double)n1/4.0)*R;
		if(n1==4) y-=30.0;
		z=((double)n1*R*(sin(SITA(n3))))/4.0;
		lpVec->x=D3DVAL(x);lpVec->y=D3DVAL(y);lpVec->z=D3DVAL(z);
		//法線ベクトルの計算
		lpNor->x=D3DVAL(-x/R);lpNor->y=D3DVAL(-y/R);lpNor->z=D3DVAL(-z/R);
		//UV値の計算
		lpVer->tu=D3DVAL(4.0*((double)n2/8.0));
		lpVer->tv=D3DVAL(4.0*(1.0-(double)n1/4.0));
		lpVer->color=D3DRGB(1,1,1);
		lpVer++;
	}

	////////////////////////////
	//天球の頂点を結合する
	s=1;
	lpPos=face_data;
	for(i=0;i<8;i++){
		//３頂点のポリゴン
		*lpPos=3;lpPos++;
		*lpPos=s;lpPos++;
		*lpPos=s+4;lpPos++;
		*lpPos=0;lpPos++;
		//４頂点のポリゴン
		s2=s;
		for(j=0;j<3;j++){
			*lpPos=4;lpPos++;
			*lpPos=s2+1;lpPos++;
			*lpPos=s2+5;lpPos++;
			*lpPos=s2+4;lpPos++;
			*lpPos=s2;lpPos++;
			s2++;
		}
		s+=4;
	}

	//空の天球のメッシュ（MeshBuilderではない)
	LPDIRECT3DRMMESH lpSkyMesh = NULL;

	//空の天球のオブジェ削除
	if(lpSkyFrame != NULL){
		lpSkyFrame->DeleteVisual(lpSkyMesh);
		DeleteFrame(lpSkyFrame);
		lpSkyFrame->Release();
	}

	////////////////////////////////
	//空の天球のオブジェ作成
	lpDirect3DRM->CreateMesh(&lpSkyMesh);
	lpSkyMesh->AddGroup(37,32,0,face_data,&id);
	lpSkyMesh->SetVertices(id,0,37,Ver);
	lpSkyMesh->SetGroupMapping(id,D3DRMMAP_PERSPCORRECT);

	//テクスチャは光を考慮しない
	lpSkyMesh->SetGroupQuality(id,D3DRMSHADE_GOURAUD | D3DRMLIGHT_OFF | D3DRMFILL_SOLID);
	lpSkyMesh->SetGroupTexture(id,lpSkytex);

	//メッシュをフレームに結びつける
	lpDirect3DRM->CreateFrame(lpD3DRMScene, &lpSkyFrame);
	lpSkyFrame->AddVisual(lpSkyMesh);

	lpSkyFrame->Release();

    lpSkyMesh->Release();

	lpSkytex->Release();

	return TRUE;
}
*/

//エラーハンドラとCOMオブジェクトの解放
void ReleaseAll(void)
{

/*
	if(lpSkyFrame != NULL)
		lpSkyFrame->Release();
*/

	int i;
	for(i=0; i < NUM; i++){
	if(lpTreeFrames[i] != NULL)
		lpTreeFrames[i]->Release();
	}

	if(lpLandFrame != NULL)
		lpLandFrame->Release();

	if(lpObjFrame != NULL)
		lpObjFrame->Release();

	if(lpD3DRMCamera != NULL)
		lpD3DRMCamera->Release();
	if(lpD3DRMScene != NULL)
		lpD3DRMScene->Release();
	if(lpD3DRMView != NULL)
		lpD3DRMView->Release();
	if(lpD3DRMDevice != NULL)
		lpD3DRMDevice->Release();
	if(lpDirect3DRM != NULL)
		lpDirect3DRM->Release();

	if(lpDirect3D != NULL)
		lpDirect3D->Release();
	if(lpD3DDevice != NULL)
		lpD3DDevice->Release();

	if(lpZbuffer != NULL)
		lpZbuffer->Release();
	if(lpPrimary != NULL)
		lpPrimary->Release();
//	if(lpBackbuffer != NULL)
//		lpBackbuffer->Release();
	if(lpDDClipper != NULL)
		lpDDClipper->Release();
	if(lpDirectDraw != NULL)
		lpDirectDraw->Release();
}
