/*
* Module:   TRTCMainViewController
*
* Function: 使用TRTC SDK完成 1v1 和 1vn 的视频通话功能
*
*    1. 支持九宫格平铺和前后叠加两种不同的视频画面布局方式，该部分由 TRTCVideoViewLayout
*       来计算每个视频画面的位置排布和大小尺寸(window暂未实现九宫格布局)
*
*    2. 支持对视频通话的分辨率、帧率和流畅模式进行调整，该部分由 TRTCSettingViewController 来实现
*
*    3. 创建或者加入某一个通话房间，需要先指定 roomid 和 userid，这部分由 TRTCLoginViewController 来实现
*/

#include "StdAfx.h"
#include "TRTCMainViewController.h"
#include "TRTCLoginViewController.h"
#include "TRTCSettingViewController.h"
#include "TRTCVideoViewLayout.h"
#include "MainViewBottomBar.h"
#include "DataCenter.h"
#include "util/Base.h"
#include "MsgBoxWnd.h"
#include "util/log.h"
#include "TXLiveAvVideoView.h"
#include "TRTCGetUserIDAndUserSig.h"


//////////////////////////////////////////////////////////////////////////TXLiveAvVideoView
//duilib要实现一些特殊的功能，需要集成布局，做成最基础布局。   
class CBaseLayoutUI : public CHorizontalLayoutUI
{
    DECLARE_DUICONTROL(CBaseLayoutUI)
public:
    CBaseLayoutUI(TRTCMainViewController *pMainWnd) {
        m_pMainWnd = pMainWnd;
    };
    ~CBaseLayoutUI() {};
    void InitBaseLayoutUI() {
        m_pBottomTool = static_cast<CVerticalLayoutUI*>(m_pMainWnd->getPaintManagerUI().FindControl(_T("layout_bottom_tool_area")));
        m_pTopTool = static_cast<CHorizontalLayoutUI*>(m_pMainWnd->getPaintManagerUI().FindControl(_T("layout_top_tool_area")));
        m_pPKLayout = static_cast<CHorizontalLayoutUI*>(m_pMainWnd->getPaintManagerUI().FindControl(_T("layout_container_pkview")));
        if (m_pTopTool)
        {
            m_pTopTool->SetFloat();
            m_pTopTool->SetFixedWidth(320);
            m_pTopTool->SetFixedHeight(60);
        }
        if (m_pPKLayout)
        {
            m_pPKLayout->SetFloat();
            m_pPKLayout->SetFixedWidth(320);
            m_pPKLayout->SetFixedHeight(220);
        }
    }
protected:
    virtual void DoEvent(TEventUI& event) {
        CControlUI::DoEvent(event);
        if (event.Type == UIEVENT_MOUSEENTER)
        {
            m_pBottomTool->SetVisible(true);
            if (m_pTopTool)
                m_pTopTool->SetVisible(true);
            
        }
        else if (event.Type == UIEVENT_MOUSELEAVE)
        {
            RECT itemRc = GetPos();
            RECT winRc = { 0 };
            POINT point;
            ::GetCursorPos(&point);
            ::GetWindowRect(m_pMainWnd->GetHWND(), &winRc);
            itemRc.right = winRc.left + itemRc.right - itemRc.left - 5;
            itemRc.left = itemRc.left + winRc.left + 5;
            itemRc.bottom = winRc.top + itemRc.bottom - itemRc.top + 5;
            itemRc.top = itemRc.top + winRc.top - 5;
            BOOL bRet = ::PtInRect(&itemRc, point);
            if (!bRet) 
            {
                m_pBottomTool->SetVisible(false);
                if (m_pTopTool)
                    m_pTopTool->SetVisible(false);
            }
        }
        else if (event.Type == UIEVENT_BUTTONUP)
        {

        }
    };
    virtual void SetPos(RECT rc, bool bNeedInvalidate /* = true */) 
    {
        CHorizontalLayoutUI::SetPos(rc, bNeedInvalidate);
        m_pBottomTool->SetFixedWidth(rc.right - rc.left - 2);

        if (m_pTopTool)
        {
            RECT rc = m_rcItem;
            int width = rc.right - rc.left;
            int left = 10; 
            int top = 4;
            SIZE leftTop = { left,top };
            SIZE btnLeftTop = m_pTopTool->GetFixedXY();
            if (btnLeftTop.cx != leftTop.cx || btnLeftTop.cy != leftTop.cy)
            {
                m_pTopTool->SetFixedXY(leftTop); 
            }
        }  
        if (m_pPKLayout)
        {
            RECT rc = m_rcItem;
            int width = rc.right - rc.left;
            int left = 270; 
            int top = rc.bottom - rc.top - 280;
            SIZE leftTop = { left,top };
            SIZE btnLeftTop = m_pPKLayout->GetFixedXY();
            if (btnLeftTop.cx != leftTop.cx || btnLeftTop.cy != leftTop.cy)
            {
                m_pPKLayout->SetFixedXY(leftTop);
            }
        }

    }
private:
    CVerticalLayoutUI *m_pBottomTool = nullptr;
    CHorizontalLayoutUI *m_pTopTool = nullptr;
    CHorizontalLayoutUI *m_pPKLayout = nullptr;
    TRTCMainViewController *m_pMainWnd = nullptr;
};

////////////////////////////////////////////////////////////////////////// CTRTCMainWnd
TRTCMainViewController::TRTCMainViewController()
{
    m_pMainViewBottomBar = new MainViewBottomBar(this);
    m_pVideoViewLayout = new TRTCVideoViewLayout();
}

TRTCMainViewController::~TRTCMainViewController()
{
    TRTCCloudCore::GetInstance()->removeSDKMsgObserverByHwnd(GetHWND());
    m_pmUI.RemoveNotifier(this);
    m_pmUI.RemoveNotifier(m_pMainViewBottomBar);
}

void TRTCMainViewController::Notify(TNotifyUI & msg)
{

}

void TRTCMainViewController::InitWindow()
{
    SetIcon(IDR_MAINFRAME);
    //初始化视频渲染窗口分配器
    if (m_pBaseLayoutUI)
        m_pBaseLayoutUI->InitBaseLayoutUI();
    m_pVideoViewLayout->initRenderUI();
    m_pMainViewBottomBar->InitBottomUI();

    TRTCCloudCore::GetInstance()->Init();

    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_EnterRoom, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_ExitRoom, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_MemberEnter, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_MemberExit, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_Error, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_Dashboard, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_DeviceChange, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_SDKEventMsg, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_ConnectionLost, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_TryToReconnect, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_ConnectionRecovery, GetHWND());
	TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_SubVideoAvailable, GetHWND());
	TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_VideoAvailable, GetHWND());
	TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_ScreenStart, GetHWND());
	TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_ScreenEnd, GetHWND());
	TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_VodStart, GetHWND());
	TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_VodEnd, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_UserVoiceVolume, GetHWND());
    //TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_UserListStaticChange, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_PKConnectStatus, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_PKDisConnectStatus, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_NetworkQuality, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_CustomVideoCapture, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_CustomAudioCapture, GetHWND());
    TRTCCloudCore::GetInstance()->regSDKMsgObserver(WM_USER_CMD_FirstVideoFrame, GetHWND());

    //设置连接环境
    bool bLinkTestServer = CDataCenter::GetInstance()->m_bLinkTestServer;
    if (bLinkTestServer)
    {
        typedef void (*PFN_setNetEnv)(bool bTestEnv);
        HMODULE hLiteAV = ::GetModuleHandleW(L"liteav.dll");
        PFN_setNetEnv pfn_setNetEnv = (PFN_setNetEnv)::GetProcAddress(hLiteAV, "setNetEnv");
        if (pfn_setNetEnv)
        {
            pfn_setNetEnv(true);
        }
    }
    else
    {
        typedef void(*PFN_setNetEnv)(bool bTestEnv);
        HMODULE hLiteAV = ::GetModuleHandleW(L"liteav.dll");
        PFN_setNetEnv pfn_setNetEnv = (PFN_setNetEnv)::GetProcAddress(hLiteAV, "setNetEnv");
        if (pfn_setNetEnv)
        {
            pfn_setNetEnv(false);
        }
    }

    //进入房间
    //进入房间
    CDataCenter::LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
    TRTCParams params;
    params.sdkAppId = TRTCGetUserIDAndUserSig::instance().getSdkAppId();
    params.roomId = info._roomId;//std::to_string(info._roomId).c_str();
    std::string userid = Ansi2UTF8(info._userId).c_str();
    params.userId = (char*)userid.c_str();
    std::string userSig = Ansi2UTF8(info._userSig).c_str();
    params.userSig = (char*)userSig.c_str();
    std::string privMapEncrypt = "";
    params.privateMapKey = (char*)privMapEncrypt.c_str();

    std::string businessInfo = "{\"Str_uc_params\":{\"pure_audio_push_mod\": 1}}";
    if (CDataCenter::GetInstance()->m_bPureAudioStyle)
        params.businessInfo = businessInfo.c_str();

    //字符串进房测试样本
    std::string formatStr;
    if (isTestStringRoomId())
    {
        params.roomId = -1;
        std::string strRoomId = std::to_string(info._roomId).c_str();
        formatStr = format("{\"strGroupId\":\"%s\"}", strRoomId.c_str());
        params.businessInfo = formatStr.c_str();
    }

    TRTCCloudCore::GetInstance()->getTRTCCloud()->enterRoom(params, CDataCenter::GetInstance()->m_sceneParams);
    //TRTCCloudCore::GetInstance()->getTRTCCloud()->callExperimentalAPI("{\"api\":\"enableAdvancedScreenCapture\",\"params\":{\"licence\":\"KSO+hOFs1q5SkEnx8bvp6wN/RY+n5xl/ZuBUH2B2utYNV2lYW1D0imxtc3d4xB/NH2UghGf3Z0dPvJLXI3rZRJ2bagAXIgoy2LsYLvnYZNUJl/zK8Yuf7Ig+MOciaBl07E5nclq5QY4vq2dz3tJHEDW/ewZwT3L0eh3xH/eXey6PCf5jGteh5u7J6al55Hc4JaDKoLFPuwx4K925afoeYJIYj0fVNh+gb4Y7PtDY3i0ep8m5HLIiMlCwiSUt9pNs5M/cK2hSh+pP4vNuFW5b+XOmrnBn0UzoE/uNNFp18OQdrai2BmAQd/t9qj2D/J4qTWV5RER+2EeJx8fZH0QRMU82phazOTlzcqF9rhKrqUFdUVZY3aHMRwcbIs9Rq3eYVntWacPUklyFWIzv9Y26ZRxsGC7mLOzYSComR+Ni9L8RBEq6VMQJUdCGYzSQh+xUdTNMfiRM2h3sKP2xJONzaAV5yHcuPBLQY+GfqtkQZ+Z99K3H/fU8lCVFEGBh/eDbNKOy55pjrCe3KyE4h7yBi9TGPAM9fMSgjdBUUvmivuEF9K3H/fU8lCVFEGBh/eDbNBJXYGSTy7v8zr7etIpuuxmCYJmqSOfAxv1Yvhh+vNuHNson6baSq8up766EEZLgoQtjiqm6e49m27sfXJopxkffPZf/SaEP+LRSByJPpjGAWL5BP28jo83wUHKNfnjnnnPvWcpDG1QUh7ESrcu3lm7uh8FUrfa0LfyvVEmovOiULR7fM17vA0m3Cg/3hUlGzom1PX0VwoV10sePJK/z/Hj1D52Syz6DuReg0ea9brKdEOxQ70VnSFdkRqmEzAOjA7TcFxXeEtwH/iDioxx6UiWu18ptfpQzHsqo8FnEO/OE\"}}");

    //此处为了sdk本地视频回调时，userid = "",做的特殊处理
    VideoCanvasContainer::localUserId = Ansi2Wide(info._userId);

    //设置默认配置到SDK
    TRTCCloudCore::GetInstance()->getTRTCCloud()->setVideoEncoderParam(CDataCenter::GetInstance()->m_videoEncParams);
    TRTCCloudCore::GetInstance()->getTRTCCloud()->setNetworkQosParam(CDataCenter::GetInstance()->m_qosParams);
    TRTCCloudCore::GetInstance()->getTRTCCloud()->setSubStreamEncoderParam(CDataCenter::GetInstance()->m_subVideoEncParams);
    TRTCCloudCore::GetInstance()->getTRTCCloud()->setLocalViewMirror(CDataCenter::GetInstance()->m_bLocalVideoMirror);
    TRTCCloudCore::GetInstance()->getTRTCCloud()->setVideoEncoderMirror(CDataCenter::GetInstance()->m_bLocalVideoMirror);

    TRTCCloudCore::GetInstance()->getTRTCCloud()->setCurrentMicDeviceVolume(CDataCenter::GetInstance()->m_micVolume);
    TRTCCloudCore::GetInstance()->getTRTCCloud()->setCurrentSpeakerVolume(CDataCenter::GetInstance()->m_speakerVolume);
    if (CDataCenter::GetInstance()->m_bShowAudioVolume)
        TRTCCloudCore::GetInstance()->getTRTCCloud()->enableAudioVolumeEvaluation(200);

    if (CDataCenter::GetInstance()->m_bCDNMixTranscoding)
        TRTCCloudCore::GetInstance()->startCloudMixStream(info._userId);


    //设置美颜到SDK
    CDataCenter::BeautyConfig& beautyConfig = CDataCenter::GetInstance()->GetBeautyConfig();
    if (beautyConfig._bOpenBeauty)
    {
        TRTCCloudCore::GetInstance()->getTRTCCloud()->setBeautyStyle(beautyConfig._beautyStyle,\
            beautyConfig._beautyValue, beautyConfig._whiteValue, beautyConfig._ruddinessValue);
    }
    else
    {
        TRTCCloudCore::GetInstance()->getTRTCCloud()->setBeautyStyle(beautyConfig._beautyStyle, 0, 0, 0);
    }
    //设置大小流
    if (CDataCenter::GetInstance()->m_bPushSmallVideo)
    {
        TRTCVideoEncParam param;
        param.videoFps = 15;
        param.videoBitrate = 100;
        param.videoResolution = TRTCVideoResolution_160_120;
        TRTCCloudCore::GetInstance()->getTRTCCloud()->enableSmallVideoStream(true, param);
    }
    if (CDataCenter::GetInstance()->m_bPlaySmallVideo)
    {
        TRTCCloudCore::GetInstance()->getTRTCCloud()->setPriorRemoteVideoStreamType(TRTCVideoStreamTypeSmall);
    }

    //打开本地预览
    int bRet = m_pVideoViewLayout->dispatchVideoView(Ansi2Wide(info._userId), TRTCVideoStreamType::TRTCVideoStreamTypeBig);
    //处理是否纯音频模式
    bool bAudioCallStyle = CDataCenter::GetInstance()->m_bPureAudioStyle;
    if (!bAudioCallStyle)
    {
        std::vector<TRTCCloudCore::MediaDeviceInfo> cameraInfo = TRTCCloudCore::GetInstance()->getCameraDevice();
        LINFO(L"CTRTCMainWnd::InitWindow() camera.size[%d], bRet[%d]\n", cameraInfo.size(), bRet);
        if (bRet == 0)
        {
            if (cameraInfo.size() > 0)
                TRTCCloudCore::GetInstance()->startPreview();
        }
    }

    TRTCCloudCore::GetInstance()->getTRTCCloud()->startLocalAudio();

    CDuiString strFormat;
    if (isTestStringRoomId())
    {
        strFormat.Format(L"%s正在进入(字符串)[%d]房间", Log::_GetDateTimeString().c_str(), info._roomId);
    }
    else
    {
        strFormat.Format(L"%s正在进入[%d]房间", Log::_GetDateTimeString().c_str(), info._roomId);
    }
    
    TXLiveAvVideoView::appendEventLogText(info._userId, TRTCVideoStreamTypeBig,strFormat.GetData());

    CheckDeviceStatus();

}

void TRTCMainViewController::CheckDeviceStatus()
{
    std::vector<TRTCCloudCore::MediaDeviceInfo> micInfo = TRTCCloudCore::GetInstance()->getMicDevice();
    std::vector<TRTCCloudCore::MediaDeviceInfo> cameraInfo = TRTCCloudCore::GetInstance()->getCameraDevice();
    std::vector<TRTCCloudCore::MediaDeviceInfo> speakerInfo = TRTCCloudCore::GetInstance()->getSpeakDevice();
    std::wstring strTip = L"Error::";
    bool bShowMsgBox = false;
    bool bMuteAudioUI = false;
    bool bMuteVideoUI = false;
    if (micInfo.size() <= 0)
    {
        bMuteAudioUI = true;
        bShowMsgBox = true;
    }
    if (cameraInfo.size() <= 0)
    {
        bShowMsgBox = true;
        bMuteVideoUI = true;
    }
    if (speakerInfo.size() <= 0)
    {
        bShowMsgBox = true;
    }
    if (bShowMsgBox)
    {
        strTip += L"请检查本地设备。";
        std::wstring * text = new std::wstring;
        text->append(strTip);
        ::PostMessage(GetHWND(), ID_DELAY_SHOW_MSGBOX, (WPARAM)text, 0);
    }

    bool bAudioCallStyle = CDataCenter::GetInstance()->m_bPureAudioStyle;
    if (bAudioCallStyle)
        bMuteVideoUI = true;
    
    CDataCenter::LocalUserInfo& _loginInfo = CDataCenter::GetInstance()->m_loginInfo;
    if (bMuteVideoUI)
    {
        _loginInfo._bMuteVideo = bMuteVideoUI;
        m_pVideoViewLayout->muteVideo(Ansi2Wide(_loginInfo._userId), TRTCVideoStreamTypeBig, _loginInfo._bMuteVideo);
        m_pMainViewBottomBar->muteLocalVideoBtn(_loginInfo._bMuteVideo);
    }

    if (bMuteAudioUI)
    {
        _loginInfo._bMuteAudio = bMuteAudioUI;
        m_pVideoViewLayout->muteAudio(Ansi2Wide(_loginInfo._userId), TRTCVideoStreamTypeBig, _loginInfo._bMuteAudio);
        m_pMainViewBottomBar->muteLocalAudioBtn(_loginInfo._bMuteAudio);
    }
}

CPaintManagerUI & TRTCMainViewController::getPaintManagerUI()
{
    return m_pmUI;
}

TRTCVideoViewLayout * TRTCMainViewController::getTRTCVideoViewLayout()
{
    return m_pVideoViewLayout;
}

void TRTCMainViewController::OnFinalMessage(HWND hWnd)
{
    delete this;
}

LRESULT TRTCMainViewController::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_CREATE) {
        m_pmUI.Init(m_hWnd);
        CDialogBuilder builder;
        CControlUI* pRoot = builder.Create(_T("trtc_mainwnd.xml"), (UINT)0, this, &m_pmUI);
        ASSERT(pRoot && "Failed to parse XML");
        m_pmUI.AttachDialog(pRoot);
        m_pmUI.AddNotifier(this);
        m_pmUI.AddNotifier(m_pMainViewBottomBar);
        InitWindow();
        return 0;
    }
    else if (uMsg == WM_CLOSE)
    {
        if (wParam != ID_CLOSE_WINDOW_NO_QUIT_MSGLOOP)
        {
            DoExitRoom();
            return false;
        }
    }
    else if (uMsg == WM_DESTROY)
    {
        //拦截退出程序消息，退回登录窗口
        //::PostQuitMessage(0L);
    }
    else if (uMsg == WM_NCACTIVATE)
    {
        if (!::IsIconic(*this)) return (wParam == 0) ? TRUE : FALSE;
    }
    else if (uMsg == WM_USER_CMD_EnterRoom)
    {
        uint32_t useTime = (uint32_t)lParam;
        onEnterRoom(useTime);
    }
    else if (uMsg == WM_USER_CMD_ExitRoom)
    {
        onExitRoom(lParam);
    }
    else if (uMsg == WM_USER_CMD_MemberEnter)
    {
        std::string * userId = (std::string *)wParam;
        onUserEnter(*userId);
        delete userId;
        userId = nullptr;

    }
    else if (uMsg == WM_USER_CMD_MemberExit)
    {
        std::string * userId = (std::string *)wParam;
        onUserExit(*userId);
        delete userId;
        userId = nullptr;
    }
    else if (uMsg == WM_USER_CMD_Error)
    {
        onError(lParam);
    }
    else if (uMsg == WM_USER_CMD_Dashboard)
    {
        //std::string * userId = (std::string *)wParam;
        //std::string * value = (std::string *)lParam;
                    
        DashboardInfo* info = (DashboardInfo*)lParam;
        onDashBoardData(info->streamType, info->userId, info->buffer);
        delete info;
        info = nullptr;

        //delete value;
        //value = nullptr;
    }
    else if (uMsg == WM_USER_CMD_SDKEventMsg)
    {
        //std::string * userId = (std::string *)wParam;
        //std::string * value = (std::string *)lParam;
        DashboardInfo* info = (DashboardInfo*)lParam;
        onSDKEventData(info->streamType, info->userId, info->buffer);
        delete info;
        info = nullptr;
        //delete value;
        //value = nullptr;
    }
    else if (uMsg == WM_USER_CMD_ConnectionLost)
    {
        CDataCenter::LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
        if (m_pVideoViewLayout)
        {
            CDuiString strFormat;
            strFormat.Format(L"%s网络异常", Log::_GetDateTimeString().c_str());
            TXLiveAvVideoView::appendEventLogText(info._userId, TRTCVideoStreamTypeBig, strFormat.GetData());
            //m_pViewDispatch->AppendEventLogText(info._userId, strFormat.GetData());
        }
    }
    else if (uMsg == WM_USER_CMD_TryToReconnect)
    {
        CDataCenter::LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
        if (m_pVideoViewLayout)
        {
            CDuiString strFormat;
            strFormat.Format(L"%s尝试重进房", Log::_GetDateTimeString().c_str());
            TXLiveAvVideoView::appendEventLogText(info._userId, TRTCVideoStreamTypeBig, strFormat.GetData());
            //m_pViewDispatch->AppendEventLogText(info._userId, strFormat.GetData());
        }
    }
    else if (uMsg == WM_USER_CMD_ConnectionRecovery)
    {
        CDataCenter::LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
        if (m_pVideoViewLayout)
        {
            CDuiString strFormat;
            strFormat.Format(L"%s网络恢复，重进房成功", Log::_GetDateTimeString().c_str());
            TXLiveAvVideoView::appendEventLogText(info._userId, TRTCVideoStreamTypeBig, strFormat.GetData());
            //m_pViewDispatch->AppendEventLogText(info._userId, strFormat.GetData());
        }
	}
	else if (uMsg == WM_USER_CMD_SubVideoAvailable)
	{
		std::string * userId = (std::string *)wParam;
		bool available = (bool)lParam;
		onSubVideoAvailable(*userId, available);
		delete userId;
		userId = nullptr;
	}
	else if (uMsg == WM_USER_CMD_VideoAvailable)
	{
		std::string * userId = (std::string *)wParam;
		bool available = (bool)lParam;
		onVideoAvailable(*userId, available);
		delete userId;
		userId = nullptr;
	}
    else if (uMsg == WM_USER_CMD_UserVoiceVolume)
    {
        std::string * userId = (std::string *)wParam;
        uint32_t volume = (uint32_t)lParam;
        onUserVoiceVolume(*userId, volume);
        delete userId;
        userId = nullptr;
    }
    /*
    else if (uMsg == WM_USER_CMD_UserListStaticChange)
    {
        std::vector<UserVideoInfo>* vecList = (std::vector<UserVideoInfo>*)wParam;
        UserVideoInfo* localInfo = (UserVideoInfo*)lParam;
        onUserVideoListChange(*vecList, *localInfo);
        vecList->clear();
        delete vecList;
        vecList = nullptr;
        delete localInfo;
        localInfo = nullptr;
    }
    */
    else if (uMsg == WM_USER_SET_SHOW_VOICEVOLUME)
    {
        bool bShow = (bool)wParam;
        if (m_pVideoViewLayout)
            m_pVideoViewLayout->updateVoiceVolume(L"", 0);
    }
    else if (uMsg == WM_USER_CMD_PKConnectStatus)
    {
        TXLiteAVError bShow = (TXLiteAVError)wParam;
        std::string * str = (std::string *)lParam;
        m_pMainViewBottomBar->onConnectOtherRoom(bShow, *str);
        delete str;
        str = nullptr;
    }
    else if (uMsg == WM_USER_CMD_PKDisConnectStatus)
    {
        TXLiteAVError bShow = (TXLiteAVError)wParam;
        std::string * str = (std::string *)lParam;
        m_pMainViewBottomBar->onDisconnectOtherRoom(bShow, *str);
        delete str;
        str = nullptr;
    }
    else if (uMsg == WM_USER_VIEW_BTN_CLICK)
    {
        UI_EVENT_MSG* msg = (UI_EVENT_MSG*)wParam;
        onViewBtnClickEvent(msg->_id, msg->_userId, msg->_streamType);
        delete msg;
        msg = nullptr;
    }
    else if (uMsg == WM_USER_CMD_NetworkQuality)
    {
        std::string * userId = (std::string *)wParam;
        int quality = (int)lParam;
        onNetworkQuality(*userId, quality);
        delete userId;
        userId = nullptr;
    }
    else if (uMsg == WM_USER_CMD_CustomVideoCapture)
    {
        bool bStart = (bool)wParam;
        if (bStart)
        {
            UINT ticket = 100;
            if (CDataCenter::GetInstance()->m_videoEncParams.videoFps > 0)
            {
                ticket= 1000 / CDataCenter::GetInstance()->m_videoEncParams.videoFps;
            }
            ::SetTimer(GetHWND(), m_nCustomVideoTimerID, ticket, NULL);
        }
        else
            ::KillTimer(GetHWND(), m_nCustomVideoTimerID);
    }
    else if (uMsg == WM_USER_CMD_CustomAudioCapture)
    {
        bool bStart = (bool)wParam;
        if (bStart)
            ::SetTimer(GetHWND(), m_nCustomAudioTimerID, 15, NULL);
        else
            ::KillTimer(GetHWND(), m_nCustomAudioTimerID);
    }
    else if (uMsg == WM_TIMER)
    {
        UINT timeid = (UINT)wParam;
        if (timeid == m_nCustomVideoTimerID)
        {
            TRTCCloudCore::GetInstance()->sendCustomVideoFrame();
            return true;
        }
        else if (timeid == m_nCustomAudioTimerID)
        {
            TRTCCloudCore::GetInstance()->sendCustomAudioFrame();
            return true;
        }

    }
    else if (uMsg == WM_USER_CMD_FirstVideoFrame)
    {
        std::string * userId = (std::string *)wParam;
        uint32_t resolution = (uint32_t)lParam;
        uint32_t width = resolution >> 16;
        uint32_t height = resolution - width << 16;
        onFirstVideoFrame(*userId, width, height);
        delete userId;
        userId = nullptr;
    }
    LRESULT lRes = 0;
    if (m_pmUI.MessageHandler(uMsg, wParam, lParam, lRes))
        return lRes;
    return CWindowWnd::HandleMessage(uMsg, wParam, lParam);
}

CControlUI* TRTCMainViewController::CreateControl(LPCTSTR pstrClass)
{
    if (_tcsicmp(pstrClass, _T("VideoCanvasContainer")) == 0)
        return m_pVideoViewLayout->CreateControl(pstrClass, &m_pmUI);

    if (_tcsicmp(pstrClass, _T("BaseLayoutUI")) == 0)
    {
        m_pBaseLayoutUI = new CBaseLayoutUI(this);
        m_pBaseLayoutUI->SetBkColor(0xFF050505);
        CDialogBuilder builder;
        CControlUI* pUi = builder.Create(_T("trtc_mainbase.xml"), (UINT)0, this);
        m_pBaseLayoutUI->Add(pUi);
        return m_pBaseLayoutUI;
    }
    return NULL;
}

void TRTCMainViewController::onEnterRoom(uint32_t useTime)
{
    TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalVideo(false);
    TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalAudio(false);

    CDataCenter::LocalUserInfo& info = CDataCenter::GetInstance()->getLocalUserInfo();
    CDuiString strFormat;
    strFormat.Format(L"%s进入[%d]房间成功,耗时:%dms", Log::_GetDateTimeString().c_str(), info._roomId, useTime);
    TXLiveAvVideoView::appendEventLogText(info._userId, TRTCVideoStreamTypeBig, strFormat.GetData());
    //m_pViewDispatch->AppendEventLogText(info._userId, strFormat.GetData());
    info._bEnterRoom = true;

    CDataCenter::GetInstance()->_localVideoInfo.userId = info._userId;
    TRTCVideoEncParam params = CDataCenter::GetInstance()->m_videoEncParams;
    long cWidth = 640, cHeight = 360;
    convertCaptureResolution(params.videoResolution, cWidth, cHeight);
    CDataCenter::GetInstance()->_localVideoInfo.width = cWidth;
    CDataCenter::GetInstance()->_localVideoInfo.height = cHeight;
    TRTCCloudCore::GetInstance()->updateMixTranCodeInfo(CDataCenter::GetInstance()->_remoteVideoInfo, CDataCenter::GetInstance()->_localVideoInfo);
}

void TRTCMainViewController::onExitRoom(int reason)
{
    CDataCenter::GetInstance()->_remoteVideoInfo.clear();
    CDataCenter::GetInstance()->CleanRoomInfo();
    TRTCCloudCore::GetInstance()->Uninit();
    TRTCLoginViewController* pLogin = new TRTCLoginViewController();
    if (pLogin == NULL) return;
    //pLogin->Create(NULL, _T("TRTCDuilibDemo"), WS_VISIBLE | WS_SYSMENU | WS_CLIPCHILDREN, WS_EX_WINDOWEDGE);
    pLogin->Create(NULL, _T("TRTCDuilibDemo"), WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, WS_EX_WINDOWEDGE);
    pLogin->CenterWindow();
    pLogin->ShowWindow(true);
    Close(ID_CLOSE_WINDOW_NO_QUIT_MSGLOOP);
}

void TRTCMainViewController::onUserEnter(std::string userId)
{
    uint32_t roomId = 0;
    m_pMainViewBottomBar->onPKUserEnterRoom(userId, roomId);

    CDataCenter::LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
    CDuiString strFormat;
    strFormat.Format(L"%s[%s]加入房间)", Log::_GetDateTimeString().c_str(), Ansi2Wide(userId).c_str());
    TXLiveAvVideoView::appendEventLogText(info._userId, TRTCVideoStreamTypeBig, strFormat.GetData());
    //m_pViewDispatch->AppendEventLogText(info._userId, strFormat.GetData());
}

void TRTCMainViewController::onUserExit(std::string userId)
{
    m_pMainViewBottomBar->onPKUserLeaveRoom(userId);
    if (TRTCCloudCore::GetInstance()->getTRTCCloud())
        TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteView(userId.c_str());
    m_pVideoViewLayout->deleteVideoView(Ansi2Wide(userId), TRTCVideoStreamType::TRTCVideoStreamTypeBig);

    //强制清除辅路视频位。
    if (TRTCCloudCore::GetInstance()->getTRTCCloud())         
        TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteSubStreamView(userId.c_str());
    m_pVideoViewLayout->deleteVideoView(Ansi2Wide(userId), TRTCVideoStreamType::TRTCVideoStreamTypeSub);

    CDataCenter::LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
    CDuiString strFormat;
    strFormat.Format(L"%s[%s]离开房间", Log::_GetDateTimeString().c_str(), Ansi2Wide(userId).c_str());
    //m_pViewDispatch->AppendEventLogText(info._userId, strFormat.GetData());
    TXLiveAvVideoView::appendEventLogText(info._userId, TRTCVideoStreamTypeBig, strFormat.GetData());
    TXLiveAvVideoView::clearUserEventLogText(userId);


    RemoteUserListMap& _remoteList = CDataCenter::GetInstance()->m_remoteUser;
    RemoteUserListMap::iterator iter;//定义一个迭代指针iter
    for (iter = _remoteList.begin(); iter != _remoteList.end();)
    {
        if (iter->first.first.compare(userId) == 0)
        {
            iter = _remoteList.erase(iter);
        }
        else
            iter++;
    }

    std::vector<UserVideoInfo>::iterator iter_vec;
    for (iter_vec = CDataCenter::GetInstance()->_remoteVideoInfo.begin(); iter_vec != CDataCenter::GetInstance()->_remoteVideoInfo.end();)
    {
        if (iter_vec->userId == userId)
        {
            iter_vec = CDataCenter::GetInstance()->_remoteVideoInfo.erase(iter_vec);
            break;
        }
        else
            iter_vec++;
    }

    TRTCVideoEncParam params = CDataCenter::GetInstance()->m_videoEncParams;
    long cWidth = 640, cHeight = 360;
    convertCaptureResolution(params.videoResolution, cWidth, cHeight);
    CDataCenter::GetInstance()->_localVideoInfo.width = cWidth;
    CDataCenter::GetInstance()->_localVideoInfo.height = cHeight;

    TRTCCloudCore::GetInstance()->updateMixTranCodeInfo(CDataCenter::GetInstance()->_remoteVideoInfo, CDataCenter::GetInstance()->_localVideoInfo);
}

void TRTCMainViewController::onSubVideoAvailable(std::string userId, bool available)
{
    //退出不会回调：onSubVideoAvailable, onUserExit必须清除状态。
	if (available) {
        RemoteUserInfo remoteInfo;
        remoteInfo._bSubscribeVideo = false;
		int bRet = m_pVideoViewLayout->dispatchVideoView(Ansi2Wide(userId), TRTCVideoStreamTypeSub);
		if (bRet == 0 && TRTCCloudCore::GetInstance()->getTRTCCloud()) {
			TRTCCloudCore::GetInstance()->getTRTCCloud()->startRemoteSubStreamView(userId.c_str(), NULL);
            remoteInfo._bSubscribeVideo = true;
        }
        std::pair<std::string, TRTCVideoStreamType> key = { userId, TRTCVideoStreamTypeSub };
        CDataCenter::GetInstance()->m_remoteUser.insert({ key, remoteInfo });

	}
	else {
		if (TRTCCloudCore::GetInstance()->getTRTCCloud())
			TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteSubStreamView(userId.c_str());
        m_pVideoViewLayout->deleteVideoView(Ansi2Wide(userId), TRTCVideoStreamTypeSub);

        RemoteUserListMap& _remoteList = CDataCenter::GetInstance()->m_remoteUser;
        RemoteUserListMap::iterator iter;//定义一个迭代指针iter
        for (iter = _remoteList.begin(); iter != _remoteList.end();)
        {
            if (iter->first == std::make_pair(userId, TRTCVideoStreamTypeSub))
            {
                iter = _remoteList.erase(iter);
                break;
            }
            else
                iter++;
        }
	}
}

void TRTCMainViewController::onVideoAvailable(std::string userId, bool available)
{
    if (available) {
        RemoteUserInfo remoteInfo;
        remoteInfo._bSubscribeVideo = false;
        int bRet = m_pVideoViewLayout->dispatchVideoView(Ansi2Wide(userId), TRTCVideoStreamTypeBig);
        if (bRet == 0 && TRTCCloudCore::GetInstance()->getTRTCCloud()) {
            TRTCCloudCore::GetInstance()->getTRTCCloud()->startRemoteView(userId.c_str(), NULL);
            remoteInfo._bSubscribeVideo = true;
        }
        std::pair<std::string, TRTCVideoStreamType> key = { userId, TRTCVideoStreamTypeBig };
        CDataCenter::GetInstance()->m_remoteUser.insert({ key, remoteInfo });

    }
    else {
        if (TRTCCloudCore::GetInstance()->getTRTCCloud())
            TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteView(userId.c_str());
        m_pVideoViewLayout->deleteVideoView(Ansi2Wide(userId), TRTCVideoStreamTypeBig);

        RemoteUserListMap& _remoteList = CDataCenter::GetInstance()->m_remoteUser;
        RemoteUserListMap::iterator iter;//定义一个迭代指针iter
        for (iter = _remoteList.begin(); iter != _remoteList.end();)
        {
            if (iter->first == std::make_pair(userId, TRTCVideoStreamTypeBig))
            {
                iter = _remoteList.erase(iter);
                break;
            }
            else
                iter++;
        }

        std::vector<UserVideoInfo>::iterator iter_vec;
        for (iter_vec = CDataCenter::GetInstance()->_remoteVideoInfo.begin(); iter_vec != CDataCenter::GetInstance()->_remoteVideoInfo.end();)
        {
            if (iter_vec->userId == userId)
            {
                iter_vec = CDataCenter::GetInstance()->_remoteVideoInfo.erase(iter_vec);
                break;
            }
            else
                iter_vec++;
        }

        TRTCVideoEncParam params = CDataCenter::GetInstance()->m_videoEncParams;
        long cWidth = 640, cHeight = 360;
        convertCaptureResolution(params.videoResolution, cWidth, cHeight);
        CDataCenter::GetInstance()->_localVideoInfo.width = cWidth;
        CDataCenter::GetInstance()->_localVideoInfo.height = cHeight;

        TRTCCloudCore::GetInstance()->updateMixTranCodeInfo(CDataCenter::GetInstance()->_remoteVideoInfo, CDataCenter::GetInstance()->_localVideoInfo);

    }
}

void TRTCMainViewController::onError(int errCode)
{
    CDataCenter::LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
    if (errCode == 2)
    {
        CDuiString strFormat;
        strFormat.Format(L"%s进入[%d]房间失败", Log::_GetDateTimeString().c_str(), info._roomId);
        TXLiveAvVideoView::appendEventLogText(info._userId, TRTCVideoStreamTypeBig, strFormat.GetData());
        //if (m_pViewDispatch)
        //    m_pViewDispatch->AppendEventLogText(info._userId, strFormat.GetData());
    }
    else
    {
        CDuiString strFormat;
        strFormat.Format(L"%sSDK返回错误码 ErrorCode = %d", Log::_GetDateTimeString().c_str(), errCode);
        TXLiveAvVideoView::appendEventLogText(info._userId, TRTCVideoStreamTypeBig, strFormat.GetData());
        //if (m_pViewDispatch)
        //    m_pViewDispatch->AppendEventLogText(info._userId, strFormat.GetData());
    }
}

void TRTCMainViewController::onDashBoardData(int streamType, std::string userId, std::string data)
{
    if (m_pVideoViewLayout)
    {
        //大小视频是占一个视频位，底层支持动态切换。
        if (streamType == TRTCVideoStreamTypeSmall)
            streamType = TRTCVideoStreamTypeBig;
        TXLiveAvVideoView::appendDashboardLogText(userId, (TRTCVideoStreamType)streamType, UTF82Wide(data));
        //m_pViewDispatch->SetDashboardLogText(UTF82Wide(userId), (TRTCVideoStreamType)streamType, UTF82Wide(data));
    }
}

void TRTCMainViewController::onSDKEventData(int streamType, std::string userId, std::string data)
{
    TXLiveAvVideoView::appendEventLogText(userId, (TRTCVideoStreamType)streamType, UTF82Wide(data));
    //if (m_pViewDispatch)
    //    m_pViewDispatch->AppendEventLogText(userId, UTF82Wide(data));
}

void TRTCMainViewController::onUserVoiceVolume(std::string userId, uint32_t volume)
{
    if (CDataCenter::GetInstance()->m_bShowAudioVolume)
    {
        if (m_pVideoViewLayout)
            m_pVideoViewLayout->updateVoiceVolume(Ansi2Wide(userId), volume);
    }
}

void TRTCMainViewController::onNetworkQuality(std::string userId, int quality)
{
    if (m_pVideoViewLayout)
        m_pVideoViewLayout->updateNetSignal(Ansi2Wide(userId), quality);
}

/*
void TRTCMainViewController::onUserVideoListChange(std::vector<UserVideoInfo> vec, UserVideoInfo& localInfo)
{
    TRTCCloudCore::GetInstance()->updateMixTranCodeInfo(vec, localInfo);
}
*/

void TRTCMainViewController::onViewBtnClickEvent(int id, std::wstring userId, int streamType)
{
    if (id == UI_EVENT_MSG::UI_BTNMSG_ID_MuteVideo)
    {
        std::wstring localUserId = Ansi2Wide(CDataCenter::GetInstance()->getLocalUserID());
        if (localUserId.compare(userId) == 0)
        {
            std::vector<TRTCCloudCore::MediaDeviceInfo> deviceInfo = TRTCCloudCore::GetInstance()->getCameraDevice();
            if (deviceInfo.size() <= 0)
            {
                CMsgWnd::ShowMessageBox(GetHWND(), _T("TRTCDuilibDemo"), _T("Error: 未检出到摄像头，请检查本地电脑设备。"), 0xFFF08080);
                return;
            }
            onLocalVideoPublishChange(userId, streamType);
        }
        else
        {
            onRemoteVideoSubscribeChange(userId, streamType);
        }


    }
    else if (id == UI_EVENT_MSG::UI_BTNMSG_ID_MuteAudio)
    {
        std::wstring localUserId = Ansi2Wide(CDataCenter::GetInstance()->getLocalUserID());
        if (localUserId.compare(userId) == 0)
        {
            std::vector<TRTCCloudCore::MediaDeviceInfo> deviceInfo = TRTCCloudCore::GetInstance()->getMicDevice();
            if (deviceInfo.size() <= 0)
            {
                CMsgWnd::ShowMessageBox(GetHWND(), _T("TRTCDuilibDemo"), _T("Error: 未检出到麦克风，请检查本地电脑设备。"), 0xFFF08080);
                return;
            }
            onLocalAudioPublishChange(userId, streamType);
        }
        else
        {
            onRemoteAudioSubscribeChange(userId, streamType);
        }
    }
}

void TRTCMainViewController::onLocalVideoPublishChange(std::wstring userId, int streamType)
{
    CDataCenter::LocalUserInfo& _loginInfo = CDataCenter::GetInstance()->m_loginInfo;
    if (streamType == TRTCVideoStreamTypeBig)
    {
        if (_loginInfo._bMuteVideo == false)
        {
            _loginInfo._bMuteVideo = !_loginInfo._bMuteVideo;
            m_pVideoViewLayout->muteVideo(userId, (TRTCVideoStreamType)streamType, _loginInfo._bMuteVideo);
            m_pMainViewBottomBar->muteLocalVideoBtn(_loginInfo._bMuteVideo);
            TRTCCloudCore::GetInstance()->stopPreview();
            TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalVideo(true);
        }
        else
        {
            _loginInfo._bMuteVideo = !_loginInfo._bMuteVideo;
            m_pVideoViewLayout->muteVideo(userId, (TRTCVideoStreamType)streamType, _loginInfo._bMuteVideo);
            m_pMainViewBottomBar->muteLocalVideoBtn(_loginInfo._bMuteVideo);
            TRTCCloudCore::GetInstance()->startPreview();
            TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalVideo(false);
        }
    }
    else if (streamType == TRTCVideoStreamTypeSub)
    {
        TRTCScreenCaptureSourceInfo info{};
        info.type = TRTCScreenCaptureSourceTypeUnknown;
        m_pMainViewBottomBar->OpenScreenBtnEvent(nullptr, info);
    }
}

void TRTCMainViewController::onLocalAudioPublishChange(std::wstring userId, int streamType)
{
    if (streamType == TRTCVideoStreamTypeBig)
    {
        CDataCenter::LocalUserInfo& _loginInfo = CDataCenter::GetInstance()->m_loginInfo;
        if (_loginInfo._bMuteAudio == false)
        {
            _loginInfo._bMuteAudio = !_loginInfo._bMuteAudio;
            m_pVideoViewLayout->muteAudio(userId, (TRTCVideoStreamType)streamType, _loginInfo._bMuteAudio);
            m_pMainViewBottomBar->muteLocalAudioBtn(_loginInfo._bMuteAudio);
            TRTCCloudCore::GetInstance()->getTRTCCloud()->stopLocalAudio();
            TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalAudio(true);
        }
        else
        {
            _loginInfo._bMuteAudio = !_loginInfo._bMuteAudio;
            m_pVideoViewLayout->muteAudio(userId, (TRTCVideoStreamType)streamType, _loginInfo._bMuteAudio);
            m_pMainViewBottomBar->muteLocalAudioBtn(_loginInfo._bMuteAudio);
            TRTCCloudCore::GetInstance()->getTRTCCloud()->startLocalAudio();
            TRTCCloudCore::GetInstance()->getTRTCCloud()->muteLocalAudio(false);
        }
    }
}

void TRTCMainViewController::onRemoteVideoSubscribeChange(std::wstring userId, int streamType)
{
    RemoteUserListMap& _remoteList = CDataCenter::GetInstance()->m_remoteUser;
    for (auto &itr : _remoteList)
    {
        if (itr.first == std::make_pair(Wide2Ansi(userId), (TRTCVideoStreamType)streamType))
        {
            if (streamType == TRTCVideoStreamTypeBig)
            {
                itr.second._bSubscribeVideo = !itr.second._bSubscribeVideo;
                m_pVideoViewLayout->muteVideo(userId, (TRTCVideoStreamType)streamType, !itr.second._bSubscribeVideo);
                if (itr.second._bSubscribeVideo)
                    TRTCCloudCore::GetInstance()->getTRTCCloud()->startRemoteView(Wide2Ansi(userId).c_str(), nullptr);
                else
                    TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteView(Wide2Ansi(userId).c_str());
            }
            else if (streamType == TRTCVideoStreamTypeSub)
            {
                itr.second._bSubscribeVideo = !itr.second._bSubscribeVideo;
                m_pVideoViewLayout->muteVideo(userId, (TRTCVideoStreamType)streamType, !itr.second._bSubscribeVideo);
                if (itr.second._bSubscribeVideo)
                    TRTCCloudCore::GetInstance()->getTRTCCloud()->startRemoteSubStreamView(Wide2Ansi(userId).c_str(), nullptr);
                else
                    TRTCCloudCore::GetInstance()->getTRTCCloud()->stopRemoteSubStreamView(Wide2Ansi(userId).c_str());
            }
            break;
        }
    }
}

void TRTCMainViewController::onRemoteAudioSubscribeChange(std::wstring userId, int streamType)
{
    RemoteUserListMap& _remoteList = CDataCenter::GetInstance()->m_remoteUser;
    for (auto &itr : _remoteList)
    {
        if (itr.first == std::make_pair(Wide2Ansi(userId), (TRTCVideoStreamType)streamType))
        {
            if (streamType == TRTCVideoStreamTypeBig)
            {
                itr.second._bSubscribeAudio = !itr.second._bSubscribeAudio;
                m_pVideoViewLayout->muteAudio(userId, (TRTCVideoStreamType)streamType, !itr.second._bSubscribeAudio);
                TRTCCloudCore::GetInstance()->getTRTCCloud()->muteRemoteAudio(Wide2Ansi(userId).c_str(), !itr.second._bSubscribeAudio);
            }
            break;
        }
    }
}

void TRTCMainViewController::DoExitRoom()
{
    CDataCenter::LocalUserInfo info = CDataCenter::GetInstance()->getLocalUserInfo();
    bool bExit = false;
    if (info._bEnterRoom == false)
    {
        LINFO(L"DoExitRoom info._bEnterRoom = false");
        bExit = true;
    }
    else
    {
        if (MSGID_OK == CMsgWnd::MessageBox(m_hWnd, _T("TRTCDuilibDemo"), _T("您确定要退出房间吗？")))
        {
            bExit = true;
            LINFO(L"DoExitRoom info._bEnterRoom = true");
        }
    }
    if (bExit)
    {
        ::KillTimer(GetHWND(), m_nCustomAudioTimerID);
        ::KillTimer(GetHWND(), m_nCustomVideoTimerID);
        TRTCCloudCore::GetInstance()->PreUninit();
        m_pMainViewBottomBar->UnInitBottomUI();
        m_pVideoViewLayout->unInitRenderUI();
        TXLiveAvVideoView::clearAllLogText();
        ShowWindow(false);
        TRTCCloudCore::GetInstance()->getTRTCCloud()->exitRoom();
    }
    if (info._bEnterRoom == false)
    {
        onExitRoom(0);
    }
}

void TRTCMainViewController::onFirstVideoFrame(std::string userId, uint32_t width, uint32_t height)
{
    bool bFind = false;
    for (auto& it : CDataCenter::GetInstance()->_remoteVideoInfo)
    {
        if (it.userId == userId)
        {
            it.width = width;
            it.height = height;
            bFind = true;
            break;
        }
    }
    if (bFind == false)
    {
        UserVideoInfo info;
        info.userId = userId;
        info.width = width;
        info.height = height;
        CDataCenter::GetInstance()->_remoteVideoInfo.push_back(info);
    }

    TRTCVideoEncParam params = CDataCenter::GetInstance()->m_videoEncParams;
    long cWidth = 640, cHeight = 360;
    convertCaptureResolution(params.videoResolution, cWidth, cHeight);
    CDataCenter::GetInstance()->_localVideoInfo.width = cWidth;
    CDataCenter::GetInstance()->_localVideoInfo.height = cHeight;

    TRTCCloudCore::GetInstance()->updateMixTranCodeInfo(CDataCenter::GetInstance()->_remoteVideoInfo, CDataCenter::GetInstance()->_localVideoInfo);
}

bool TRTCMainViewController::isTestStringRoomId()
{
    WCHAR fileBuffer[MAX_PATH] = { 0 };

    ::GetModuleFileNameW(NULL, fileBuffer, MAX_PATH);

    std::wstring path(fileBuffer);
    if (path.size() > 0)
    {
        int pos = path.find_last_of('\\', path.length());
        std::wstring fileDir = path.substr(0, pos);  // Return the directory without the file name   
        fileDir += L"\\TestStringRoomId.txt";

        if ((_waccess(fileDir.c_str(), 0)) != -1)
        {
            return true;
        }
    }

    return false;
}

void TRTCMainViewController::getSizeAlign16(long originWidth, long originHeight, long& align16Width, long& align16Height)
{
    // 转换16对齐的宽高
    // 宽度和高度能被16整除，并尽量保留宽高比

#if defined(_WIN32)
    assert(originWidth > 0 && originHeight > 0);
#endif

    // 640x360 或 360x640，通常换算为640x368 或 368x640，但实际656x368 或 368x656更加接近原始宽高比，做下适配
    if (640 == originWidth && 360 == originHeight)
    {
        align16Width = 640;
        align16Height = 368;

        return;
    }
    else if (360 == originWidth && 640 == originHeight)
    {
        align16Width = 368;
        align16Height = 640;

        return;
    }

    bool isWidthAlign16 = (0 == originWidth % 16);
    bool isHeightAlign16 = (0 == originHeight % 16);
    if (isWidthAlign16 && isHeightAlign16) {
        align16Width = originWidth;
        align16Height = originHeight;
    }
    else if (!isWidthAlign16 && isHeightAlign16) {
        align16Width = (originWidth + 15) / 16 * 16;

        double originRatio = (double)originWidth / originHeight;
        double modRatio = (double)align16Width / originHeight;
        double add16Ratio = (double)align16Width / (originHeight + 16);
        if (std::fabs(modRatio - originRatio) <= std::fabs(add16Ratio - originRatio)) {
            align16Height = originHeight;
        }
        else {
            align16Height = originHeight + 16;
        }
    }
    else if (isWidthAlign16 && !isHeightAlign16) {
        align16Height = (originHeight + 15) / 16 * 16;

        double originRatio = (double)originWidth / originHeight;
        double modRatio = (double)originWidth / align16Height;
        double add16Ratio = (double)(originWidth + 16) / align16Height;
        if (std::fabs(modRatio - originRatio) <= std::fabs(add16Ratio - originRatio)) {
            align16Width = originWidth;
        }
        else {
            align16Width = originWidth + 16;
        }
    }
    else { // !isWidthAlign16 && !isHeightAlign16
        align16Width = (originWidth + 15) / 16 * 16;
        align16Height = (originHeight + 15) / 16 * 16;
    }
}

void TRTCMainViewController::convertCaptureResolution(TRTCVideoResolution resolution, long& width, long& height)
{
    switch (resolution)
    {
    case TRTCVideoResolution_120_120:
        getSizeAlign16(120, 120, width, height);
        break;
    case TRTCVideoResolution_160_160:
        getSizeAlign16(160, 160, width, height);
        break;
    case TRTCVideoResolution_270_270:
        getSizeAlign16(270, 270, width, height);
        break;
    case TRTCVideoResolution_480_480:
        getSizeAlign16(480, 480, width, height);
        break;
    case TRTCVideoResolution_160_120:
        getSizeAlign16(160, 120, width, height);
        break;
    case TRTCVideoResolution_240_180:
        getSizeAlign16(240, 180, width, height);
        break;
    case TRTCVideoResolution_280_210:
        getSizeAlign16(280, 210, width, height);
        break;
    case TRTCVideoResolution_320_240:
        getSizeAlign16(320, 240, width, height);
        break;
    case TRTCVideoResolution_400_300:
        getSizeAlign16(400, 300, width, height);
        break;
    case TRTCVideoResolution_480_360:
        getSizeAlign16(480, 360, width, height);
        break;
    case TRTCVideoResolution_640_480:
        getSizeAlign16(640, 480, width, height);
        break;
    case TRTCVideoResolution_960_720:
        getSizeAlign16(960, 720, width, height);
        break;
    case TRTCVideoResolution_160_90:
        getSizeAlign16(160, 90, width, height);
        break;
    case TRTCVideoResolution_256_144:
        getSizeAlign16(256, 144, width, height);
        break;
    case TRTCVideoResolution_320_180:
        getSizeAlign16(320, 180, width, height);
        break;
    case TRTCVideoResolution_480_270:
        getSizeAlign16(480, 270, width, height);
        break;
    case TRTCVideoResolution_640_360:
        getSizeAlign16(640, 360, width, height);
        break;
    case TRTCVideoResolution_960_540:
        getSizeAlign16(960, 540, width, height);
        break;
    case TRTCVideoResolution_1280_720:
        getSizeAlign16(1280, 720, width, height);
        break;
    case TRTCVideoResolution_1920_1080:
        getSizeAlign16(1920, 1080, width, height);
        break;
    default:
        assert(false);
        break;
    }
}