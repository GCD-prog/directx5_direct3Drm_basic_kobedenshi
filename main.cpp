#include <windows.h>
#include <stdio.h>
#include <math.h>

#include <ddraw.h>
//#include <d3drm.h>
#include <d3drmwin.h>
#include <mmsystem.h>	//�}���`���f�B�A�n�̐���itimeGetTime()�ŕK�v�j

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

//�t���[������]������
void SetFrameRot(LPDIRECT3DRMFRAME lpFrame, int rx, int ry, int rz);
//�t���[���̈ʒu�w��
void SetFramePos(LPDIRECT3DRMFRAME lpFrame, double x, double y, double z);
//�J�����̉�]
void SetCameraRot(int rx, int ry, int rz);
//�J�����̈ʒu�w��
void SetCameraPos(double x, double y, double z);

//�V�[�����t���[�����폜
void DeleteFrame(LPDIRECT3DRMFRAME lpFrame);

BOOL SetLight(void);

BOOL SetObject(void);

BOOL CreateTreeMesh(void);

BOOL AddLandscape(void);

// BOOL AddSkyMesh(void);

void ReleaseAll(void);

struct tagPLAYER{
	double x,y,z;		//�����̈ʒu
	double view_x, view_y, view_z;	// ���_�̕��ʊp: view_x , ���_�̋p: view_y , ���_�ƒ����_�̋���: view_z
	int rx,ry,rz;		//������X,Y��]�p
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

		 // �J�����̂ݕ\������
		 lpBackbuffer->GetDC(&hdc);
		 TextOut(hdc, 0, 0, text0, strlen(text0));

		 TextOut(hdc, 0, 20, Data, strlen(Data));
		 lpBackbuffer->ReleaseDC(hdc);
}

void KeyControl(void)
{
	//��������������

	ry = (Player.ry -90 + 360) % 360;	//Player.ry��0�Ȃ琳�ʂ������Ă���

	fx = cos(SITA(ry));
	fz = -sin(SITA(ry));

	//��
	if(GetAsyncKeyState('Z')&0x8000){
		Player.y += 2;
	}

	//��
	if(GetAsyncKeyState('X')&0x8000){
		Player.y -= 2;
	}

	//���_����ɌX����
	if(GetAsyncKeyState(VK_UP)&0x8000){
		if(Player.rx > -30) Player.rx -= 2;
	}

	//���_�����ɌX����
	if(GetAsyncKeyState(VK_DOWN)&0x8000){		
		if(Player.rx < 30) Player.rx += 2;
	}

	//������
	if( (GetAsyncKeyState(VK_LEFT)&0x8000) || (GetAsyncKeyState('Q')&0x8000) ){
		Player.rz -= 2;
		Player.ry -= 2;
	}

	//�E����
	if( (GetAsyncKeyState(VK_RIGHT)&0x8000) || (GetAsyncKeyState('E')&0x8000) ){
		Player.rz += 2;
		Player.ry += 2;
	}

	//�O�i
	if(GetAsyncKeyState('W')&0x8000){
		Player.x += fx;
		Player.z += fz;
	}

	//���
	if(GetAsyncKeyState('S')&0x8000){
		Player.x -= fx;
		Player.z -= fz;
	}

	//��
	if(GetAsyncKeyState('A')&0x8000){
		Player.x -= fz;
	}

	//�E
	if(GetAsyncKeyState('D')&0x8000){
		Player.x += fz;
	}

	//���_�̈ʒu�ƕ������X�V
	Player.ry = (Player.ry + 360) % 360;		//���̒l��0-359�̒l

}

BOOL RenderFrame(void)
{

	if ( lpPrimary->IsLost() == DDERR_SURFACELOST )		lpPrimary->Restore();

		RECT Scrrc={0, 0, 640, 480};   //��ʂ̃T�C�Y

		//�b�ԂU�O�t���[�����z���Ȃ��悤�ɂ���
		static DWORD nowTime, prevTime;
		nowTime = timeGetTime();
		if( (nowTime - prevTime) < 1000 / 60 ) return 0;
		prevTime = nowTime;

		//�L�[����
		KeyControl();

		// --- �I�u�W�F�N�g�X�V ---
	    // �ʒu�X�V (�V�[���)
		SetFramePos(lpD3DRMCamera, Player.x, Player.y, Player.z);

		//�J�����t���[���̈ړ� yaw��Y���Apitch��X���Aroll��Z���̉�]
		SetCameraPos(Player.x, Player.y, Player.z);
		SetCameraRot((Player.rx + 360) % 360, Player.ry, 0);		//FPS���_�̉�]

		//�؂̍쐬
		int i;
		for(i = 0; i < NUM; i++){
			//�؂����_�̕����Ɍ������� , �r���{�[�h����
			ry = (Player.ry - 180 + 360) % 360;
			SetFrameRot(lpTreeFrames[i], 0, ry, 0);

/*
			if (lpTreeFrames[i]) {					//lpTreeFrames[i] ���L���ȏꍇ�̂ݍ폜
				DeleteFrame(lpTreeFrames[i]);		//�����̃I�u�W�F�N�g�t���[�����V�[������폜���A���
				lpTreeFrames[i] = NULL;				//�폜�����̂Ń|�C���^��NULL�ɂ���
			}
*/

		}

		// --- �n�ʂ̍Ĕz�u ���̃��W�b�N���A�v���C���[�̈ʒu�ɍ��킹�Ēn�ʂ��č쐬���邱�Ƃ�
		// �n�ʂ���O�ɓ����Ă���悤�Ɍ�������ʂ𐶂ݏo���܂��B
		if (lpLandFrame) {				//lpLandFrame ���L���ȏꍇ�̂ݍ폜
			DeleteFrame(lpLandFrame);	//�����̒n�ʃt���[�����V�[������폜���A���
			lpLandFrame = NULL;			//�폜�����̂Ń|�C���^��NULL�ɂ���
		}
		AddLandscape();					//�V�����n�ʃt���[�����v���C���[�̌��݈ʒu����ɍ쐬

		//Direct3DRM �����_�����O����
		lpD3DRMScene->Move(D3DVAL(1.0)); 
		lpD3DRMView->Clear();
		
		// 2D�X�v���C�g��\������ꍇ�����ɋL�q
		//�w�i���o�b�N�o�b�t�@�ɓ]��
		// lpBackbuffer->BltFast(0, 0, lpScreen, &Scrrc, DDBLTFAST_NOCOLORKEY | DDBLTFAST_WAIT);

		lpD3DRMView->Render(lpD3DRMScene);
		lpD3DRMDevice->Update();

		//FPS�l�v��
		FrameCnt();

		// Flip�֐��͎g��Ȃ� ��ʂ������
		// lpPrimary->Flip(NULL, DDFLIP_WAIT);

		//  Flip�֐����g��Ȃ��t���b�v����
		RECT rc;
		SetRect(&rc, 0, 0, 640, 480);  //  ��ʑS�̂��w��
		lpPrimary->BltFast(0, 0, lpBackbuffer, NULL, DDBLTFAST_NOCOLORKEY);

		return TRUE;
}

//-------------------------------------------
// �h���C�o���������֐�
//-------------------------------------------
GUID* D3D_GuidSearch(HWND hwnd)
{
	HRESULT d3dret;   //�_�C���N�g�R�c�n�֐��̖߂�l���i�[����ϐ�
	GUID*   Guid;
	LPDIRECT3D          lpD3D;
	LPDIRECTDRAW        lpDD;
	D3DFINDDEVICESEARCH S_DATA;
	D3DFINDDEVICERESULT R_DATA;
	char str[100];

	//GUID�擾�̏���
	memset(&S_DATA, 0, sizeof S_DATA);
	S_DATA.dwSize = sizeof S_DATA;
	S_DATA.dwFlags = D3DFDS_COLORMODEL;
	S_DATA.dcmColorModel = D3DCOLOR_RGB;
	memset(&R_DATA, 0, sizeof R_DATA);
	R_DATA.dwSize = sizeof R_DATA;

	//DIRECTDRAW�̐���
	d3dret = DirectDrawCreate(NULL, &lpDD, NULL);
	if (d3dret != DD_OK) {
		MessageBox( hwnd, "�_�C���N�g�h���[�I�u�W�F�N�g�̐����Ɏ��s���܂���", "������", MB_OK);
		lpDD->Release();
		return NULL;
	}

	//DIRECTD3D�̐���
	d3dret = lpDD->QueryInterface(IID_IDirect3D, (void**)&lpD3D);
	if (d3dret != D3D_OK) {
		MessageBox( hwnd, "�_�C���N�g�R�c�I�u�W�F�N�g�̐����Ɏ��s���܂���", "������", MB_OK);
		lpDD->Release();
		lpD3D->Release();
		return NULL;
	}
	//�f�o�C�X�̗�
	d3dret = lpD3D->FindDevice(&S_DATA, &R_DATA);
	if (d3dret != D3D_OK) {
		MessageBox( hwnd, "�f�o�C�X�̗񋓂Ɏ��s���܂���", "������", MB_OK);
		lpDD->Release();
		lpD3D->Release();
		return NULL;
	}

	//�K�C�h�̎擾
	Guid = &R_DATA.guid;
	//�s�v�ɂȂ����I�u�W�F�N�g�̃����[�X
	lpDD->Release();
	lpD3D->Release();
	//OutputDebugString(str);
	wsprintf(str, "%x", *Guid);
	return (Guid);
}

/*-------------------------------------------
// DirectDraw �f�o�C�X�̗񋓂ƑI��
--------------------------------------------*/
BOOL CALLBACK DDEnumCallback(GUID FAR* lpGUID, LPSTR lpDriverDesc, LPSTR lpDriverName, LPVOID lpContext)
{
	LPDIRECTDRAW lpDD;
	DDCAPS DriverCaps, HELCaps;

	// DirectDraw �I�u�W�F�N�g�������I�ɐ�������
	if(DirectDrawCreate(lpGUID, &lpDD, NULL) != DD_OK) {
		*(LPDIRECTDRAW*)lpContext = NULL;
		return DDENUMRET_OK;
	}

	// DirectDraw�̔\�͂��擾
	ZeroMemory(&DriverCaps, sizeof(DDCAPS));
	DriverCaps.dwSize = sizeof(DDCAPS);
	ZeroMemory(&HELCaps, sizeof(DDCAPS));
	HELCaps.dwSize = sizeof(DDCAPS);

	if(lpDD->GetCaps(&DriverCaps, &HELCaps) == DD_OK) {
	// �n�[�h�E�F�A3D�x�������҂ł���ꍇ�ŁD
	// �Ȃ����e�N�X�`�����g�p�ł���ꍇ������g��
		if ((DriverCaps.dwCaps & DDCAPS_3D) && (DriverCaps.ddsCaps.dwCaps & DDSCAPS_TEXTURE)) {
			*(LPDIRECTDRAW*)lpContext = lpDD;
			lstrcpy(szDDDeviceName, lpDriverDesc);
			return DDENUMRET_CANCEL;
		}
	}

	// ���̃h���C�o������
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

				// Esc�L�[�Ńv���O�������I�����܂�
		if(wParam == VK_ESCAPE){

			//��ʃ��[�h�����ɖ߂�
			lpDirectDraw->SetCooperativeLevel(hwnd, DDSCL_NORMAL);
			lpDirectDraw->RestoreDisplayMode();

			lpScreen->Release();

			ReleaseAll(); //�e�I�u�W�F�N�g��ReleaseAll()�ŉ������

			PostQuitMessage(0);

		}

		break;

	case WM_DESTROY:

			lpScreen->Release();

			ReleaseAll(); //�e�I�u�W�F�N�g��ReleaseAll()�ŉ������

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

		ShowCursor(FALSE); //�J�[�\�����B��

		//Direct3DRM�̍\�z
		Direct3DRMCreate(&lpDirect3DRM);

		//DirectDrawClipper�̍\�z
		DirectDrawCreateClipper(0, &lpDDClipper, NULL);

		// DirectDraw�h���C�o��񋓂���
		DirectDrawEnumerate(DDEnumCallback, &lpDirectDraw);

		// �񋓂ɂ����DirectDraw�h���C�o�����߂�
		// ���肵�Ȃ������ꍇ�A���݃A�N�e�B�u�ȃh���C�o���g��
		if(!lpDirectDraw){
			lstrcpy(szDDDeviceName, "Active Driver");
			DirectDrawCreate(NULL, &lpDirectDraw, NULL);
		}

		lpDirectDraw->SetCooperativeLevel(hwnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT);

		//�f�B�X�v���C���[�h�ύX
		lpDirectDraw->SetDisplayMode(640, 480, 16);

		//��{�T�[�t�F�X�ƃt�����g�o�b�t�@�̐��� (�P���쐬)
		ZeroMemory(&Dds, sizeof(DDSURFACEDESC));
		Dds.dwSize = sizeof(DDSURFACEDESC);
		Dds.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		Dds.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY | DDSCAPS_3DDEVICE;
		Dds.dwBackBufferCount = 1;

		lpDirectDraw->CreateSurface(&Dds, &lpPrimary, NULL);

		//�o�b�N�o�b�t�@�̃|�C���^�擾
		Ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
		lpPrimary->GetAttachedSurface(&Ddscaps, &lpBackbuffer);

		// Z-Buffer�쐬
		//��{�T�[�t�F�X�ƃo�b�t�@�P���쐬
		ZeroMemory(&Dds, sizeof(DDSURFACEDESC));
		Dds.dwSize = sizeof(DDSURFACEDESC);
		Dds.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_ZBUFFERBITDEPTH;
		Dds.dwHeight = 640;
		Dds.dwWidth = 480;
		Dds.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | DDSCAPS_VIDEOMEMORY;
		Dds.dwZBufferBitDepth = 16;

		lpDirectDraw->CreateSurface(&Dds, &lpZbuffer, NULL);

		lpBackbuffer->AddAttachedSurface(lpZbuffer);

		//DirectDraw�ł̃A�N�Z�X���ł���悤�ɁAClipper������
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

		//�w�i�T�[�t�F�X���쐬
		Dds.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		Dds.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		Dds.dwWidth = 640;
		Dds.dwHeight = 480;
		lpDirectDraw->CreateSurface(&Dds, &lpScreen, NULL);

		//�J���[�L�[�̎w�� ��:RGB(255, 255, 255)�@��:RGB(0, 0, 0)
		ddck.dwColorSpaceLowValue=RGB(0, 0, 0);
		ddck.dwColorSpaceHighValue=RGB(0, 0, 0);
		lpScreen->SetColorKey(DDCKEY_SRCBLT, &ddck);

		//�e�T�[�t�F�X�ɉ摜��ǂݍ���
		LoadBMP(lpScreen, "datafile\\back.BMP");  //�w�i

		DirectDrawCreateClipper(0, &lpDDClipper, NULL);

//Direct3DRM�̏����� ��������

//�f�o�C�X�̐��� �g�d�k���s RAMP ��掿 ���x�x��
//		Direct3DRMCreate(&lpDirect3DRM);
//		lpDirect3DRM->CreateDeviceFromSurface(NULL, lpDirectDraw, lpBackbuffer, &lpD3DRMDevice);
//

//�f�o�C�X�̐��� �g�`�k�Ŏ��s����

		HRESULT ddret;   //�_�C���N�g�R�c�n�֐��̖߂�l���i�[����ϐ�
		GUID*   Guid;

		Direct3DRMCreate(&lpDirect3DRM);

		Guid = D3D_GuidSearch(hwnd);
		// HAL
		ddret = lpDirect3DRM->CreateDeviceFromSurface(Guid, (IDirectDraw*)lpDirectDraw, lpBackbuffer, &lpD3DRMDevice);
			strcpy(szDevice,"D3D HAL");
		if (ddret != D3DRM_OK) {
			MessageBox( hwnd, "�f�o�C�X�̐����Ɏ��s�A�g�`�k�ł̎��s�͕s�\�ł�", "", MB_OK);
			//HAL�ł̎��s���s�\�Ȏ��AHEL�ł̎��s���s��
			ddret = lpDirect3DRM->CreateDeviceFromSurface(Guid, (IDirectDraw*)lpDirectDraw, lpBackbuffer, &lpD3DRMDevice);
			if (ddret != D3DRM_OK) {
				strcpy(szDevice,"HEL");
				MessageBox( hwnd, "�g�d�k�ł́A�f�o�C�X�̐����Ɏ��s�ADirect3D�̎g�p�͕s�\�ł�", "", MB_OK);
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

		//�J�������쐬
								// �e�t���[�� ,	�q�t���[��
		lpDirect3DRM->CreateFrame(lpD3DRMScene, &lpD3DRMCamera);
	
		//�J�����̈ʒu��ݒ�
		Player.x = 0.0;
		Player.y = 3.0;
		Player.z = 0.0;
		Player.ry = 0;

		SetCameraPos(Player.x, Player.y, Player.z);
		SetCameraRot(0, Player.ry, 0);

		//�f�o�C�X�ƃJ��������Direct3DRMViewPort���쐬
		lpDirect3DRM->CreateViewport(lpD3DRMDevice, lpD3DRMCamera, 0, 0, 640, 480, &lpD3DRMView);
		lpD3DRMView->SetBack(D3DVAL(5000.0));

	//�Q�[���Ɋւ��鏉���������i�J�n�j---------------------------

		SetLight();

		SetObject();

		CreateTreeMesh();

		AddLandscape();

		// AddSkyMesh(void)

	//�Q�[���Ɋւ��鏉���������i�I���j---------------------------

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

		//�Q�[���Ɋւ���I�������i�J�n�j---------------------------


		//�Q�[���Ɋւ���I�������i�I���j---------------------------
}

//�t���[���̈ʒu�ړ�
void SetFramePos(LPDIRECT3DRMFRAME lpFrame, double x, double y, double z)
{
	lpFrame->SetPosition(lpD3DRMScene, D3DVAL(x), D3DVAL(y), D3DVAL(z));
	lpFrame->Release();
}

//�t���[������]������
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
	//�x���̉�]��̃x�N�g��
	y1 = d1 * cosz - cosx * sinz;
	y2 = d1 * sinz + cosx * cosz;
	y3 = sinx * cosy;
	//�y���̉�]��̃x�N�g��
	z1 = d2 * cosz + sinx * sinz;
	z2 = d2 * sinz - sinx * cosz;
	z3 = cosx * cosy;

	lpFrame->SetOrientation(lpD3DRMScene, D3DVAL(z1), D3DVAL(z2), D3DVAL(z3), D3DVAL(y1), D3DVAL(y2), D3DVAL(y3));

	lpFrame->Release();
}

//�J�����̉�]
void SetCameraRot(int rx, int ry, int rz){
	SetFrameRot(lpD3DRMCamera, rx, ry, rz);
}

//�J�����̈ʒu�w��
void SetCameraPos(double x, double y, double z){	
	SetFramePos(lpD3DRMCamera, x, y, z);
}

/*
//���W�̕ϊ��i���[���h���W�|���X�N���[�����W�ցj
//(wx,wy,wz) ... ���[���h���W��̂P�_
//(rx,ry)  ... �X�N���[�����W�ɕϊ����ꂽ���W���i�[����
BOOL Transform(double wx,double wy,double wz,int *rx,int *ry)
{
	D3DVECTOR SrcV;
	D3DRMVECTOR4D DstV;
	int x, y;
	
	SrcV.x = D3DVAL(wx);
	SrcV.y = D3DVAL(wy);
	SrcV.z = D3DVAL(wz);
	//���[���h���W�|���X�N���[�����W�ɕϊ�
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

//�V�[�����t���[�����폜
void DeleteFrame(LPDIRECT3DRMFRAME lpFrame)
{
	if(lpFrame == NULL) return;
	lpD3DRMScene->DeleteChild(lpFrame);

}

//�����̐ݒ�
BOOL SetLight(void)
{

	//�A���r�G���g������z�u
	LPDIRECT3DRMLIGHT lpD3DRMLightAmbient;
	
	lpDirect3DRM->CreateLightRGB(D3DRMLIGHT_AMBIENT, D3DVAL(5.0), D3DVAL(5.0), D3DVAL(5.0), &lpD3DRMLightAmbient);
	lpD3DRMScene->AddLight(lpD3DRMLightAmbient);
	lpD3DRMLightAmbient->Release();

	LPDIRECT3DRMFRAME lpD3DRMLightFrame;
	lpDirect3DRM->CreateFrame(lpD3DRMScene, &lpD3DRMLightFrame);
	
	//�|�C���g������z�u
	LPDIRECT3DRMLIGHT lpD3DRMLightPoint;

	lpDirect3DRM->CreateLightRGB(D3DRMLIGHT_POINT, D3DVAL(0.9), D3DVAL(0.9), D3DVAL(0.9), &lpD3DRMLightPoint);
	
	lpD3DRMLightFrame->SetPosition(lpD3DRMScene, D3DVAL(10.0), D3DVAL(0.0), D3DVAL(0.0));
	lpD3DRMLightFrame->AddLight(lpD3DRMLightPoint);

	lpD3DRMLightPoint->Release();

	lpD3DRMLightFrame->Release();

	return TRUE;

}

//�I�u�W�F�N�g�̃��[�h�iX�t�@�C���̓ǂݍ��݁jSennsya.x
BOOL SetObject(void)
{

	LPDIRECT3DRMTEXTURE lpTex = NULL;

	LPDIRECT3DRMWRAP lpWrap = NULL;

	//Direct3DRMMeshBuilder���쐬
	LPDIRECT3DRMMESHBUILDER lpObj_mesh = NULL;

	//���b�V�������݂���΍폜
	if(lpObj_mesh != NULL){
		lpObjFrame->DeleteVisual(lpObj_mesh);		//���b�V�����t���[���������
		DeleteFrame(lpObjFrame);					//�t���[�����V�[������폜����
		lpObjFrame->Release();
	}

	lpDirect3DRM->CreateMeshBuilder(&lpObj_mesh);

	// Sennsya.x ��ǂݍ���
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

	//X�t�@�C���Ƀe�N�X�`���𒣂�t��
	lpDirect3DRM->LoadTexture("datafile\\hello.bmp", &lpTex);

	lpTex->SetShades(16);

	lpObj_mesh->SetTexture(lpTex);

	lpDirect3DRM->CreateFrame(lpD3DRMCamera, &lpObjFrame);

	// SetPosition() �Q�ƃt���[������Ƃ���t���[���̈ʒu��ݒ�
	lpObjFrame->SetPosition(lpD3DRMScene, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(10.0));

	lpObjFrame->SetOrientation(lpD3DRMScene, D3DVAL(0.0), D3DVAL(0.0), D3DVAL(1.0), D3DVAL(0.0), D3DVAL(1.0), D3DVAL(0.0));

	lpObjFrame->AddVisual((LPDIRECT3DRMVISUAL)lpObj_mesh);

	lpObjFrame->Release();

	lpObj_mesh->Release();

	lpWrap->Release();

	lpTex->Release();

	return TRUE;

}

//�n�ʂ̃��b�V�����쐬����
//�P�|���S���ŕ\�� 
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


	//���_�̂P�ڂ͎��_��菭������̂Ƃ���
	ry=Player.ry-90;
	ry=(ry+360)%360;
	x=ex-(10.0*cos(SITA(ry)));
	z=ez-(-10.0*sin(SITA(ry)));
	face_ver[0].x=D3DVAL(x);face_ver[0].y=D3DVAL(0);face_ver[0].z=D3DVAL(z);

	//���_�Q��
	ry=Player.ry-90-45;
	ry=(ry+360)%360;
	x=(int)(200.0*cos(SITA(ry)))+ex;
	z=(int)(-200.0*sin(SITA(ry)))+ez;
	face_ver[1].x=D3DVAL(x);face_ver[1].y=D3DVAL(0);face_ver[1].z=D3DVAL(z);

	//���_�R��
	ry=Player.ry-90+45;
	ry=(ry+360)%360;
	x=(int)(200.0*cos(SITA(ry)))+ex;
	z=(int)(-200.0*sin(SITA(ry)))+ez;
	face_ver[2].x=D3DVAL(x);face_ver[2].y=D3DVAL(0);face_ver[2].z=D3DVAL(z);

	//������Ԑݒ�
	DWORD face_data[]={3,0,0,1,1,2,2,0};

	//�e���_���Ƃ̖@���̐ݒ�
	D3DVECTOR face_normal[]={
		D3DVAL(0),D3DVAL(1),D3DVAL(0),
		D3DVAL(0),D3DVAL(1),D3DVAL(0),
		D3DVAL(0),D3DVAL(1),D3DVAL(0),
	};

	//���b�V���̐���
	LPDIRECT3DRMMESHBUILDER lpLand_mesh = NULL;

	//�n�ʂ̃��b�V�������݂���΍폜
	if(lpLand_mesh!=NULL){
		lpLandFrame->DeleteVisual(lpLand_mesh);		//���b�V�����t���[���������
		DeleteFrame(lpLandFrame);					//�t���[�����V�[������폜����
		lpLandFrame->Release();
	}

	//���b�V���̐���
	lpDirect3DRM->CreateMeshBuilder(&lpLand_mesh);
	lpLand_mesh->AddFaces(3,face_ver,3,face_normal,face_data,&lpFaceA);
	lpLand_mesh->SetPerspective(TRUE);

	//�e�N�X�`�����|���S���ɒ���t����
	//scale�����������邱�ƂŃe�N�X�`���͂��ׂ����Ȃ�
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

	//�t���[�����쐬���A�n�ʂ̃��b�V�������т���
	lpDirect3DRM->CreateFrame(lpD3DRMScene, &lpLandFrame);

	lpLandFrame->AddVisual(lpLand_mesh);

	lpLandFrame->Release();

    lpLand_mesh->Release();

	lpLandtex->Release();

	return TRUE;
}

//�؂̃��b�V�����쐬����@1�|���S���i�����ɂ͂R�p�`�|���S���Q�j�ő���
BOOL CreateTreeMesh(void)
{
	int i;
	int x, y, z;

	LPDIRECT3DRMTEXTURE lpTreeTex;		//�؂̃e�N�X�`���p

	lpDirect3DRM->LoadTexture("datafile\\tree.bmp", &lpTreeTex);

	//�e�N�X�`���̓��ߐF�i�F�𓧖��Ƃ���j�̐ݒ�
	lpTreeTex->SetDecalTransparentColor( D3DRGB( D3DVAL(0.0), D3DVAL(0.0), D3DVAL(1.0) ));
	lpTreeTex->SetDecalTransparency(TRUE);

	//////////////////////////////////
	//	�؂̃��b�V���쐬			//
	//////////////////////////////////
	LPDIRECT3DRMFACEARRAY lpFaceA;
	LPDIRECT3DRMFACE lpFace;

	//�؂̒��_���W�ݒ�
	D3DVECTOR face_ver[] = {
		D3DVAL(10), D3DVAL(16), D3DVAL(0),
		D3DVAL(-10), D3DVAL(16), D3DVAL(0),
		D3DVAL(-10), D3DVAL(0), D3DVAL(0),
		D3DVAL(10), D3DVAL(0), D3DVAL(0)
	};
	//������Ԑݒ�
	DWORD face_data[] = {4,0,0,1,1,2,2,3,3,0};
	//�e���_���Ƃ̖@���̐ݒ�
	D3DVECTOR face_normal[] = {
		D3DVAL(0), D3DVAL(1), D3DVAL(0),
		D3DVAL(0), D3DVAL(1), D3DVAL(0),
		D3DVAL(0), D3DVAL(1), D3DVAL(0),
		D3DVAL(0), D3DVAL(1), D3DVAL(0)
	};

	LPDIRECT3DRMMESHBUILDER lpTree_mesh;	//�؂̌`��p

	//���b�V������
	lpDirect3DRM->CreateMeshBuilder(&lpTree_mesh);
	lpTree_mesh->AddFaces(4, face_ver, 4, face_normal, face_data, &lpFaceA);
	//�e�N�X�`���̂䂪�݂��łȂ��悤�ɂ���
	lpTree_mesh->SetPerspective(TRUE);

	//�e�N�X�`�����|���S���ɒ���t����
	lpFaceA->GetElement(0,&lpFace);
	lpFace->SetColor(D3DRGB(1.0, 1.0, 1.0));
	lpFace->SetTextureCoordinates(0, D3DVAL(0), D3DVAL(1.0));
	lpFace->SetTextureCoordinates(1, D3DVAL(1.0), D3DVAL(1.0));
	lpFace->SetTextureCoordinates(2, D3DVAL(1.0), D3DVAL(0));
	lpFace->SetTextureCoordinates(3, D3DVAL(0), D3DVAL(0));
	lpFace->SetTexture(lpTreeTex);
	lpFace->Release();

	//�I�u�W�F�폜
	for(i=0; i < NUM; i++){
		if(lpTreeFrames[i] != NULL){
			lpTreeFrames[i]->DeleteVisual(lpTree_mesh);	//���b�V�����t���[���������
			DeleteFrame(lpTreeFrames[i]);	//�t���[�����V�[������폜����
			lpTreeFrames[i]->Release();
		}
	}

	//�؂�z�u����
	for(i=0; i < NUM; i++){
		//�t���[�����쐬���A�؂̃��b�V�������т���
								// �e�t���[�� ,	�q�t���[��
		lpDirect3DRM->CreateFrame(lpD3DRMScene, &lpTreeFrames[i]);

		//���b�V�����t���[���Ɍ��т���
		lpTreeFrames[i]->AddVisual(lpTree_mesh);

		//�t���[���̈ʒu��ݒ�
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
//��̓V���̃��b�V���𐶐�����
//37���_(1+4*8+4)�A32�|���S��(4*8)�̔����𑢂�
BOOL AddSkyMesh(void)
{

	LPDIRECT3DRMTEXTURE lpSkytex = NULL;    

	lpDirect3DRM->LoadTexture("datafile\\cloud.bmp", &lpSkytex);

	int n1,n2,n3;
	int i,j,s,s2;
	double R=320.0;		//���̔��a
	double x,y,z;
	D3DRMVERTEX Ver[37];
	D3DRMVERTEX *lpVer;
	D3DVECTOR *lpVec;
	D3DVECTOR *lpNor;
	unsigned int face_data[32*5];
	unsigned int *lpPos;
	D3DRMGROUPINDEX id;

	///////////////////////////////////////
	//�V���̒��_���쐬����
	lpVer=Ver;
	lpVec=&(lpVer->position);
	lpNor=&(lpVer->normal);
	//�Ă��؂�̍��W��0�Ƃ���
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
		//���_���W�̌v�Z
		x=((double)n1*R*(cos(SITA(n3))))/4.0;
		y=(1.0-(double)n1/4.0)*R;
		if(n1==4) y-=30.0;
		z=((double)n1*R*(sin(SITA(n3))))/4.0;
		lpVec->x=D3DVAL(x);lpVec->y=D3DVAL(y);lpVec->z=D3DVAL(z);
		//�@���x�N�g���̌v�Z
		lpNor->x=D3DVAL(-x/R);lpNor->y=D3DVAL(-y/R);lpNor->z=D3DVAL(-z/R);
		//UV�l�̌v�Z
		lpVer->tu=D3DVAL(4.0*((double)n2/8.0));
		lpVer->tv=D3DVAL(4.0*(1.0-(double)n1/4.0));
		lpVer->color=D3DRGB(1,1,1);
		lpVer++;
	}

	////////////////////////////
	//�V���̒��_����������
	s=1;
	lpPos=face_data;
	for(i=0;i<8;i++){
		//�R���_�̃|���S��
		*lpPos=3;lpPos++;
		*lpPos=s;lpPos++;
		*lpPos=s+4;lpPos++;
		*lpPos=0;lpPos++;
		//�S���_�̃|���S��
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

	//��̓V���̃��b�V���iMeshBuilder�ł͂Ȃ�)
	LPDIRECT3DRMMESH lpSkyMesh = NULL;

	//��̓V���̃I�u�W�F�폜
	if(lpSkyFrame != NULL){
		lpSkyFrame->DeleteVisual(lpSkyMesh);
		DeleteFrame(lpSkyFrame);
		lpSkyFrame->Release();
	}

	////////////////////////////////
	//��̓V���̃I�u�W�F�쐬
	lpDirect3DRM->CreateMesh(&lpSkyMesh);
	lpSkyMesh->AddGroup(37,32,0,face_data,&id);
	lpSkyMesh->SetVertices(id,0,37,Ver);
	lpSkyMesh->SetGroupMapping(id,D3DRMMAP_PERSPCORRECT);

	//�e�N�X�`���͌����l�����Ȃ�
	lpSkyMesh->SetGroupQuality(id,D3DRMSHADE_GOURAUD | D3DRMLIGHT_OFF | D3DRMFILL_SOLID);
	lpSkyMesh->SetGroupTexture(id,lpSkytex);

	//���b�V�����t���[���Ɍ��т���
	lpDirect3DRM->CreateFrame(lpD3DRMScene, &lpSkyFrame);
	lpSkyFrame->AddVisual(lpSkyMesh);

	lpSkyFrame->Release();

    lpSkyMesh->Release();

	lpSkytex->Release();

	return TRUE;
}
*/

//�G���[�n���h����COM�I�u�W�F�N�g�̉��
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
