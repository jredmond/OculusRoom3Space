/************************************************************************************

Filename    :   Win32_OculusRoomTiny.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   October 4, 2012
Authors     :   Michael Antonov, Andrew Reisse

Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#include "Win32_OculusRoomTiny.h"
#include "RenderTiny_D3D1X_Device.h"

//-------------------------------------------------------------------------------------

//ThreeSpace Stuff
//header file
#include "ThreeSpaceAPI/yei_threespace_api.h"

//When getting stream data use a packed structure
#pragma pack(push,1)
typedef struct {
    float quat[4];
} tss_stream_packet;
#pragma pack(pop)

//The device id
TSS_Device_Id tss_device;
bool tss_isStreaming = false;
tss_stream_packet tss_packet;
unsigned int tss_timestamp;




//-------------------------------------------------------------------------------------
// ***** OculusRoomTiny Class

// Static pApp simplifies routing the window function.
OculusRoomTinyApp* OculusRoomTinyApp::pApp = 0;


OculusRoomTinyApp::OculusRoomTinyApp(HINSTANCE hinst)
    : pRender(0),
      LastUpdate(0),
            
      // Win32
      hWnd(NULL),
      hInstance(hinst), Quit(0), MouseCaptured(true),    
      hXInputModule(0), pXInputGetState(0),
      
      // Initial location
      EyePos(0.0f, 1.6f, -5.0f),
      EyeYaw(YawInitial), EyePitch(0), EyeRoll(0),
      LastSensorYaw(0),
      SConfig(),
      PostProcess(PostProcess_Distortion),
      ShiftDown(false),
      ControlDown(false)
{
    pApp = this;

    Width  = 1280;
    Height = 800;

    StartupTicks = OVR::Timer::GetTicks();
    LastPadPacketNo = 0;
   
    MoveForward   = MoveBack = MoveLeft = MoveRight = 0;
    GamepadMove   = Vector3f(0);
    GamepadRotate = Vector3f(0);
}

OculusRoomTinyApp::~OculusRoomTinyApp()
{
	RemoveHandlerFromDevices();
    pSensor.Clear();
    pHMD.Clear();
    destroyWindow();
    pApp = 0;
}

int OculusRoomTinyApp::OnStartup(const char* args)
{
    OVR_UNUSED(args);

    
    // *** ThreeSpace initialisation
    //TSS_Error tss_error;
    TSS_ComPort tss_comport;

    LARGE_INTEGER tss_frequency; //ticks per second
    LARGE_INTEGER tss_t1, tss_t2; //ticks
    QueryPerformanceFrequency(&tss_frequency);
    QueryPerformanceCounter(&tss_t1);
    QueryPerformanceCounter(&tss_t2);

    unsigned int tss_serial;

    if(tss_getComPorts(&tss_comport,1,0,TSS_FIND_ALL_KNOWN^TSS_FIND_DNG))
    {
        tss_device = tss_createTSDeviceStr(tss_comport.com_port, TSS_TIMESTAMP_SENSOR);
        if(tss_device == TSS_NO_DEVICE_ID)
        {
            LogText("Failed to create a sensor on %s\n", tss_comport.com_port);
        }
        else
        {
            if(tss_getSerialNumber(tss_device, &tss_serial, NULL) == TSS_NO_ERROR)
                LogText("Connected to ThreeSpace sensor!! Port: %s Serial: %x\n", 
                    tss_comport.com_port, tss_serial);
        }
    }
    else
    {
        LogText("No sensors found\n");
    }
    // ***

    // *** Set ThreeSpace axis directions SetTSAxisDirections()
    TSS_Axis_Direction axis_order = TSS_XZY;
    char neg_x = 1;
    char neg_y = 1;
    char neg_z = 0;
    unsigned char axis_dir_byte = tss_generateAxisDirections(axis_order, neg_x, neg_y, neg_z);
    if( tss_setAxisDirections(tss_device, axis_dir_byte, &tss_timestamp) == 0 )
        LogText("TSS: Set axis complete!\n");
    else
        LogText("TSS: Set axis failed!\n");
    // ***

    /*
    // *** Get the quat for debug information GetTSQuat()
    TSS_Error tss_error_id;
    float quat[4];
    tss_error_id = tss_getTaredOrientationAsQuaternion(tss_device, quat, &tss_timestamp);
    LogText("Quaternion: %f, %f, %f, %f Timestamp=%u\n", quat[0], quat[1], quat[2] ,quat[3], tss_timestamp);
    // *** 

    // *** Get the colour of the headtrackers LED
    LogText("Getting the LED colour of the device\n");
    float tss_color[3];
    tss_error_id = tss_getLEDColor(tss_device, tss_color, NULL);
    LogText("Color: %f, %f, %f\n", tss_color[0], tss_color[1], tss_color[2]);

    //Try setting the colour
    for(int i = 0; i <= 100; i++)
    {
        tss_color[i%3] = (float)i/100;
        tss_color[(i+1)%3] = 0;
        tss_color[(i+2)%3] = 0;
        tss_error_id = tss_setLEDColor(tss_device, tss_color, NULL);
        Sleep(100);
    }
    // ***
    */

    // *** StartStreaming
    TSS_Stream_Command tss_stream_slots[8] = { TSS_GET_TARED_ORIENTATION_AS_QUATERNION, TSS_NULL,
                                               TSS_NULL, TSS_NULL,
                                               TSS_NULL, TSS_NULL,
                                               TSS_NULL, TSS_NULL};

    int count = 0;
    if(!tss_isStreaming)
    {
        //3 Attempts
        while( count < 3)
        {
            if(tss_setStreamingTiming(tss_device,0, TSS_INFINITE_DURATION, 0, NULL) == 0)
            {
                if(tss_setStreamingSlots(tss_device, tss_stream_slots, NULL) == 0)
                {
                    if(tss_startStreaming(tss_device, NULL) == 0)
                    {
                        tss_isStreaming =  true;
                        LogText("TSS: Start streaming success!\n");
                        break;
                    }
                }
            }
            count++;
        }
    }
    if(!tss_isStreaming)
    {
        LogText("TSS: Start streaming failed!\n");
    }
    // ***

    // *** tareSensor
    tss_tareWithCurrentOrientation(tss_device,NULL);
    /*
    float forward[3];
    float down[3];
    float filt_orient_quat[4];
    float gravity[3];
    gravity[0] = 0;
    gravity[1] = 0;
    gravity[2] = -1;

    tss_getUntaredTwoVectorInSensorFrame(tss_device, forward, down);
    tss_getUntaredOrientationAsQuaternion(tss_device, filt_orient_quat);
    */

    // ***

    /*
    // *** GetData
    tss_getLatestStreamData(tss_device,(char*)&tss_packet,sizeof(tss_packet),1000,&tss_timestamp);
    LogText("t:%8u Euler: %f, %f, %f, \n", tss_timestamp,
                                           tss_packet.euler[0], 
                                           tss_packet.euler[1],
                                           tss_packet.euler[2]);
    // ***
    */

    // *** Oculus HMD & Sensor Initialization

    // Create DeviceManager and first available HMDDevice from it.
    // Sensor object is created from the HMD, to ensure that it is on the
    // correct device.

    pManager = *DeviceManager::Create();

	// We'll handle it's messages in this case.
	pManager->SetMessageHandler(this);


    int         detectionResult = IDCONTINUE;
    const char* detectionMessage;

    do 
    {
        // Release Sensor/HMD in case this is a retry.
        pSensor.Clear();
        pHMD.Clear();
        RenderParams.MonitorName.Clear();

        pHMD  = *pManager->EnumerateDevices<HMDDevice>().CreateDevice();
        if (pHMD)
        {
            pSensor = *pHMD->GetSensor();

            // This will initialize HMDInfo with information about configured IPD,
            // screen size and other variables needed for correct projection.
            // We pass HMD DisplayDeviceName into the renderer to select the
            // correct monitor in full-screen mode.
            if (pHMD->GetDeviceInfo(&HMDInfo))
            {            
                RenderParams.MonitorName = HMDInfo.DisplayDeviceName;
                RenderParams.DisplayId = HMDInfo.DisplayId;
                SConfig.SetHMDInfo(HMDInfo);
            }
        }
        else
        {            
            // If we didn't detect an HMD, try to create the sensor directly.
            // This is useful for debugging sensor interaction; it is not needed in
            // a shipping app.
            pSensor = *pManager->EnumerateDevices<SensorDevice>().CreateDevice();
        }


        // If there was a problem detecting the Rift, display appropriate message.
        detectionResult  = IDCONTINUE;        

        if (!pHMD && !pSensor)
            detectionMessage = "Oculus Rift not detected.";
        else if (!pHMD)
            detectionMessage = "Oculus Sensor detected; HMD Display not detected.";
        else if (!pSensor)
            detectionMessage = "Oculus HMD Display detected; Sensor not detected.";
        else if (HMDInfo.DisplayDeviceName[0] == '\0')
            detectionMessage = "Oculus Sensor detected; HMD display EDID not detected.";
        else
            detectionMessage = 0;

        if (detectionMessage)
        {
            String messageText(detectionMessage);
            messageText += "\n\n"
                           "Press 'Try Again' to run retry detection.\n"
                           "Press 'Continue' to run full-screen anyway.";

            detectionResult = ::MessageBoxA(0, messageText.ToCStr(), "Oculus Rift Detection",
                                            MB_CANCELTRYCONTINUE|MB_ICONWARNING);

            if (detectionResult == IDCANCEL)
                return 1;
        }

    } while (detectionResult != IDCONTINUE);

    if (HMDInfo.HResolution > 0)
    {
        Width  = HMDInfo.HResolution;
        Height = HMDInfo.VResolution;
    }


    if (!setupWindow())
        return 1;
    
    if (pSensor)
    {
        // We need to attach sensor to SensorFusion object for it to receive 
        // body frame messages and update orientation. SFusion.GetOrientation() 
        // is used in OnIdle() to orient the view.
        SFusion.AttachToSensor(pSensor);
        SFusion.SetDelegateMessageHandler(this);
        SFusion.SetPredictionEnabled(true);
    }

    
    // *** Initialize Rendering
   
    // Enable multi-sampling by default.
    RenderParams.Multisample = 4;
    RenderParams.Fullscreen  = true;

    // Setup Graphics.
    pRender = *RenderTiny::D3D10::RenderDevice::CreateDevice(RenderParams, (void*)hWnd);
    if (!pRender)
        return 1;


    // *** Configure Stereo settings.

    SConfig.SetFullViewport(Viewport(0,0, Width, Height));
    SConfig.SetStereoMode(Stereo_LeftRight_Multipass);

    // Configure proper Distortion Fit.
    // For 7" screen, fit to touch left side of the view, leaving a bit of invisible
    // screen on the top (saves on rendering cost).
    // For smaller screens (5.5"), fit to the top.
    if (HMDInfo.HScreenSize > 0.0f)
    {
        if (HMDInfo.HScreenSize > 0.140f) // 7"
            SConfig.SetDistortionFitPointVP(-1.0f, 0.0f);
        else
            SConfig.SetDistortionFitPointVP(0.0f, 1.0f);
    }

    pRender->SetSceneRenderScale(SConfig.GetDistortionScale());

    SConfig.Set2DAreaFov(DegreeToRad(85.0f));


    // *** Populate Room Scene

    // This creates lights and models.
    PopulateRoomScene(&Scene, pRender);


    LastUpdate = GetAppTime();
    return 0;
}

void OculusRoomTinyApp::OnMessage(const Message& msg)
{
	if (msg.Type == Message_DeviceAdded && msg.pDevice == pManager)
	{
		LogText("DeviceManager reported device added.\n");
	}
	else if (msg.Type == Message_DeviceRemoved && msg.pDevice == pManager)
	{
		LogText("DeviceManager reported device removed.\n");
	}
	else if (msg.Type == Message_DeviceAdded && msg.pDevice == pSensor)
	{
		LogText("Sensor reported device added.\n");
	}
	else if (msg.Type == Message_DeviceRemoved && msg.pDevice == pSensor)
	{
		LogText("Sensor reported device removed.\n");
	}
}


void OculusRoomTinyApp::OnGamepad(float padLx, float padLy, float padRx, float padRy)
{
    GamepadMove   = Vector3f(padLx * padLx * (padLx > 0 ? 1 : -1),
                             0,
                             padLy * padLy * (padLy > 0 ? -1 : 1));
    GamepadRotate = Vector3f(2 * padRx, -2 * padRy, 0);
}

void OculusRoomTinyApp::OnMouseMove(int x, int y, int modifiers)
{
    OVR_UNUSED(modifiers);

    // Mouse motion here is always relative.
    int         dx = x, dy = y; 
    const float maxPitch = ((3.1415f/2)*0.98f);

    // Apply to rotation. Subtract for right body frame rotation,
    // since yaw rotation is positive CCW when looking down on XZ plane.
    EyeYaw   -= (Sensitivity * dx)/ 360.0f;

    if (!pSensor)
    {
        EyePitch -= (Sensitivity * dy)/ 360.0f;
        
        if (EyePitch > maxPitch)
            EyePitch = maxPitch;
        if (EyePitch < -maxPitch)
            EyePitch = -maxPitch;
    }    
}

void OculusRoomTinyApp::OnKey(unsigned vk, bool down)
{
    switch (vk)
    {
    case 'Q':
        if (down && ControlDown)
            Quit = true;
        break;
    case VK_ESCAPE:
        if (!down)
            Quit = true;
        break;

    // Handle player movement keys.
    // We just update movement state here, while the actual translation is done in OnIdle()
    // based on time.
    case 'W':      MoveForward = down ? (MoveForward | 1) : (MoveForward & ~1); break;
    case 'S':      MoveBack    = down ? (MoveBack    | 1) : (MoveBack    & ~1); break;
    case 'A':      MoveLeft    = down ? (MoveLeft    | 1) : (MoveLeft    & ~1); break;
    case 'D':      MoveRight   = down ? (MoveRight   | 1) : (MoveRight   & ~1); break;
    case VK_UP:    MoveForward = down ? (MoveForward | 2) : (MoveForward & ~2); break;
    case VK_DOWN:  MoveBack    = down ? (MoveBack    | 2) : (MoveBack    & ~2); break;

    case 'R':
        SFusion.Reset();
        break;
    
    case 'P':
        if (down)
        {
            // Toggle chromatic aberration correction on/off.
            RenderDevice::PostProcessShader shader = pRender->GetPostProcessShader();

            if (shader == RenderDevice::PostProcessShader_Distortion)
            {
                pRender->SetPostProcessShader(RenderDevice::PostProcessShader_DistortionAndChromAb);                
            }
            else if (shader == RenderDevice::PostProcessShader_DistortionAndChromAb)
            {
                pRender->SetPostProcessShader(RenderDevice::PostProcessShader_Distortion);                
            }
            else
                OVR_ASSERT(false);
        }
        break;

    // Switch rendering modes/distortion.
    case VK_F1:
        SConfig.SetStereoMode(Stereo_None);
        PostProcess = PostProcess_None;
        break;
    case VK_F2:
        SConfig.SetStereoMode(Stereo_LeftRight_Multipass);
        PostProcess = PostProcess_None;
        break;
    case VK_F3:
        SConfig.SetStereoMode(Stereo_LeftRight_Multipass);
        PostProcess = PostProcess_Distortion;
        break;

    // Stereo IPD adjustments, in meter (default IPD is 64mm).    
    case VK_OEM_PLUS:    
    case VK_INSERT:
        if (down)
            SConfig.SetIPD(SConfig.GetIPD() + 0.0005f * (ShiftDown ? 5.0f : 1.0f));
        break;
    case VK_OEM_MINUS:
    case VK_DELETE:
        if (down)
            SConfig.SetIPD(SConfig.GetIPD() - 0.0005f * (ShiftDown ? 5.0f : 1.0f));
        break;

    // Holding down Shift key accelerates adjustment velocity.
    case VK_SHIFT:
        ShiftDown = down;
        break;
    case VK_CONTROL:
        ControlDown = down;
        break;
    }
}


void OculusRoomTinyApp::OnIdle()
{
    double curtime = GetAppTime();
    float  dt      = float(curtime - LastUpdate);
    LastUpdate     = curtime;


    // Handle Sensor motion.
    // We extract Yaw, Pitch, Roll instead of directly using the orientation
    // to allow "additional" yaw manipulation with mouse/controller.
    if (pSensor)
    {        
        Quatf    hmdOrient = SFusion.GetOrientation();
        float    yaw = 0.0f;

        hmdOrient.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&yaw, &EyePitch, &EyeRoll);

        EyeYaw += (yaw - LastSensorYaw);
        LastSensorYaw = yaw;    
    }    

    //Threespace sensor integration
    if(tss_isStreaming)
    {
        int error = tss_getLatestStreamData(tss_device,(char*)&tss_packet,sizeof(tss_packet),1000,&tss_timestamp);
        if(error != 0)
        {
            LogText("TSS: getLatestStreamData error\n");
        }

        const float PI_F = 3.14159265358979f;

        float x, y, z, w, s;
        float rotator[3];

        x = tss_packet.quat[0];
        y = tss_packet.quat[1];
        z = tss_packet.quat[2];
        w = tss_packet.quat[3];
        s = 2.0f * (w * y - x * z);

        //it is invalid to pass values outside of the range -1,1 to asin()
        if( s < 1.0 )
        {
            if( -1.0 < s)
            {
                rotator[0] = atan2( 2.0f*(x*y+w*z), 1.0f-2.0f*(y*y+z*z) );
                rotator[1] = asin( s );
                rotator[2] = atan2( 2.0f*(y*z+w*x), 1.0f-2.0f*(x*x+y*y) ); 
            }
            else
            {
                rotator[0] = 0;
                rotator[1] = -PI_F / 2;
                rotator[2] = -atan2( 2.0f*(x*y-w*z), 1.0f-2.0f*(x*x+z*z) );
            }
        }
        else
        {
            rotator[0] = 0;
            rotator[1] = PI_F / 2;
            rotator[2] = atan2( 2.0f*(x*y-w*z), 1.0f-2.0f*(x*x+z*z) );
        }

        float yaw = 0.0;
        yaw = rotator[0];
        EyePitch = rotator[1];
        EyeRoll = rotator[2];

        //we are allowing combination of gamepad yaw and headtracker yaw therefore
        EyeYaw += (yaw - LastSensorYaw);
        LastSensorYaw = yaw;


        /*
        float yaw = 0.0f;

        yaw = tss_packet.euler[0];
        EyePitch = tss_packet.euler[1];
        EyeRoll = tss_packet.euler[2];

        //we are allowing combination of gamepad yaw and headtracker yaw therefore
        EyeYaw += (yaw - LastSensorYaw);
        LastSensorYaw = yaw;
        */
    }

    // Gamepad rotation.
    EyeYaw -= GamepadRotate.x * dt;

    if (!pSensor && !tss_isStreaming)
    {
        // Allow gamepad to look up/down, but only if there is no Rift sensor.
        EyePitch -= GamepadRotate.y * dt;

        const float maxPitch = ((3.1415f/2)*0.98f);
        if (EyePitch > maxPitch)
            EyePitch = maxPitch;
        if (EyePitch < -maxPitch)
            EyePitch = -maxPitch;
    }
    
    // Handle keyboard movement.
    // This translates EyePos based on Yaw vector direction and keys pressed.
    // Note that Pitch and Roll do not affect movement (they only affect view).
    if (MoveForward || MoveBack || MoveLeft || MoveRight)
    {
        Vector3f localMoveVector(0,0,0);
        Matrix4f yawRotate = Matrix4f::RotationY(EyeYaw);

        if (MoveForward)
            localMoveVector = ForwardVector;
        else if (MoveBack)
            localMoveVector = -ForwardVector;

        if (MoveRight)
            localMoveVector += RightVector;
        else if (MoveLeft)
            localMoveVector -= RightVector;

        // Normalize vector so we don't move faster diagonally.
        localMoveVector.Normalize();
        Vector3f orientationVector = yawRotate.Transform(localMoveVector);
        orientationVector *= MoveSpeed * dt * (ShiftDown ? 3.0f : 1.0f);

        EyePos += orientationVector;
    }

    else if (GamepadMove.LengthSq() > 0)
    {
        Matrix4f yawRotate = Matrix4f::RotationY(EyeYaw);
        Vector3f orientationVector = yawRotate.Transform(GamepadMove);
        orientationVector *= MoveSpeed * dt;
        EyePos += orientationVector;
    }


    // Rotate and position View Camera, using YawPitchRoll in BodyFrame coordinates.
    // 
    Matrix4f rollPitchYaw = Matrix4f::RotationY(EyeYaw) * Matrix4f::RotationX(EyePitch) *
                            Matrix4f::RotationZ(EyeRoll);
    Vector3f up      = rollPitchYaw.Transform(UpVector);
    Vector3f forward = rollPitchYaw.Transform(ForwardVector);

    
    // Minimal head modelling.
    float headBaseToEyeHeight     = 0.15f;  // Vertical height of eye from base of head
    float headBaseToEyeProtrusion = 0.09f;  // Distance forward of eye from base of head

    Vector3f eyeCenterInHeadFrame(0.0f, headBaseToEyeHeight, -headBaseToEyeProtrusion);
    Vector3f shiftedEyePos = EyePos + rollPitchYaw.Transform(eyeCenterInHeadFrame);
    shiftedEyePos.y -= eyeCenterInHeadFrame.y; // Bring the head back down to original height

    View = Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + forward, up); 

    // This is what transformation would be without head modeling.    
    // View = Matrix4f::LookAtRH(EyePos, EyePos + forward, up);    

    switch(SConfig.GetStereoMode())
    {
    case Stereo_None:
        Render(SConfig.GetEyeRenderParams(StereoEye_Center));
        break;

    case Stereo_LeftRight_Multipass:
        Render(SConfig.GetEyeRenderParams(StereoEye_Left));
        Render(SConfig.GetEyeRenderParams(StereoEye_Right));
        break;
    }
     
    pRender->Present();
    // Force GPU to flush the scene, resulting in the lowest possible latency.
    pRender->ForceFlushGPU();
}


// Render the scene for one eye.
void OculusRoomTinyApp::Render(const StereoEyeParams& stereo)
{
    pRender->BeginScene(PostProcess);

    // Apply Viewport/Projection for the eye.
    pRender->ApplyStereoParams(stereo);    
    pRender->Clear();
    pRender->SetDepthMode(true, true);
    
    Scene.Render(pRender, stereo.ViewAdjust * View);

    pRender->FinishScene();
}


//-------------------------------------------------------------------------------------
// ***** Win32-Specific Logic

bool OculusRoomTinyApp::setupWindow()
{

    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpszClassName = L"OVRAppWindow";
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = systemWindowProc;
    wc.cbWndExtra    = sizeof(OculusRoomTinyApp*);
    RegisterClass(&wc);
   

    RECT winSize = { 0, 0, Width, Height };
    AdjustWindowRect(&winSize, WS_POPUP, false);
    hWnd = CreateWindowA("OVRAppWindow", "OculusRoomTiny", WS_POPUP|WS_VISIBLE,
                         HMDInfo.DesktopX, HMDInfo.DesktopY,
                         winSize.right-winSize.left, winSize.bottom-winSize.top,
                         NULL, NULL, hInstance, (LPVOID)this);


    // Initialize Window center in screen coordinates
    POINT center = { Width / 2, Height / 2 };
    ::ClientToScreen(hWnd, &center);
    WindowCenter = center;


    return (hWnd != NULL);
}

void OculusRoomTinyApp::destroyWindow()
{    
    pRender.Clear();

    if (hWnd)
    {
        // Release window resources.
        ::DestroyWindow(hWnd);
        UnregisterClass(L"OVRAppWindow", hInstance);
        hWnd = 0;
        Width = Height = 0; 
    }
}


LRESULT CALLBACK OculusRoomTinyApp::systemWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCREATE)
        pApp->hWnd = hwnd;
    return pApp->windowProc(msg, wp, lp);
}

void OculusRoomTinyApp::giveUsFocus(bool setFocus)
{
    if (setFocus)    
    {
        ::SetCursorPos(WindowCenter.x, WindowCenter.y);

        MouseCaptured = true;
        ::SetCapture(hWnd);
        ::ShowCursor(FALSE);

    }
    else
    {
        MouseCaptured = false;
        ::ReleaseCapture();
        ::ShowCursor(TRUE);
    }
}

LRESULT OculusRoomTinyApp::windowProc(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_MOUSEMOVE:
        {
            if (MouseCaptured)
            {
                // Convert mouse motion to be relative (report the offset and re-center).
                POINT newPos = { LOWORD(lp), HIWORD(lp) };
                ::ClientToScreen(hWnd, &newPos);
                if ((newPos.x == WindowCenter.x) && (newPos.y == WindowCenter.y))
                    break;
                ::SetCursorPos(WindowCenter.x, WindowCenter.y);

                LONG dx = newPos.x - WindowCenter.x;
                LONG dy = newPos.y - WindowCenter.y;           
                pApp->OnMouseMove(dx, dy, 0);
            }
        }
        break;

    case WM_MOVE:
        {
            RECT r;
            GetClientRect(hWnd, &r);
            WindowCenter.x = r.right/2;
            WindowCenter.y = r.bottom/2;
            ::ClientToScreen(hWnd, &WindowCenter);
        }
        break;

    case WM_KEYDOWN:
        OnKey((unsigned)wp, true);
        break;
    case WM_KEYUP:
        OnKey((unsigned)wp, false);
        break;

    case WM_SETFOCUS:
        giveUsFocus(true);
        break;

    case WM_KILLFOCUS:
        giveUsFocus(false);
        break;

    case WM_CREATE:
        // Hack to position mouse in fullscreen window shortly after startup.
        SetTimer(hWnd, 0, 100, NULL);
        break;

    case WM_TIMER:
        KillTimer(hWnd, 0);
        giveUsFocus(true);
        break;

    case WM_QUIT:
    case WM_CLOSE:
        Quit = true;
        return 0;
    }

    return DefWindowProc(hWnd, msg, wp, lp);
}

static inline float GamepadStick(short in)
{
    float v;
    if (abs(in) < 9000)
        return 0;
    else if (in > 9000)
        v = (float) in - 9000;
    else
        v = (float) in + 9000;
    return v / (32767 - 9000);
}

static inline float GamepadTrigger(BYTE in)
{
    return (in < 30) ? 0.0f : (float(in-30) / 225);
}


int OculusRoomTinyApp::Run()
{
    // Loop processing messages until Quit flag is set,
    // rendering game scene inside of OnIdle().

    while (!Quit)
    {
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // Read game-pad.
            XINPUT_STATE xis;

            if (pXInputGetState && !pXInputGetState(0, &xis) &&
                (xis.dwPacketNumber != LastPadPacketNo))
            {
                OnGamepad(GamepadStick(xis.Gamepad.sThumbLX),
                          GamepadStick(xis.Gamepad.sThumbLY),
                          GamepadStick(xis.Gamepad.sThumbRX),
                          GamepadStick(xis.Gamepad.sThumbRY));
                //pad.LT = GamepadTrigger(xis.Gamepad.bLeftTrigger);
                LastPadPacketNo = xis.dwPacketNumber;
            }

            pApp->OnIdle();

            // Keep sleeping when we're minimized.
            if (IsIconic(hWnd))
                Sleep(10);
        }
    }

    return 0;
}


//-------------------------------------------------------------------------------------
// ***** Program Startup

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR inArgs, int)
{
    int exitCode = 0;

    // Initializes LibOVR. This LogMask_All enables maximum logging.
    // Custom allocator can also be specified here.
    OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));

    // Scope to force application destructor before System::Destroy.
    {
        OculusRoomTinyApp app(hinst);
        //app.hInstance = hinst;

        exitCode = app.OnStartup(inArgs);
        if (!exitCode)
        {
            // Processes messages and calls OnIdle() to do rendering.
            exitCode = app.Run();
        }
    }


    // *** StopStreaming
    int count = 0;
    if(tss_isStreaming)
    {
        //3 Attempts
        while( count < 3)
        {
            if(tss_stopStreaming(tss_device, NULL) == 0)
            {
                tss_isStreaming =  false;
                LogText("TSS: Stop streaming success!\n");
                break;
            }
            count++;
        }
    }
    if(tss_isStreaming)
    {
        LogText("TSS: Stop streaming failed!\n");
    }
    // ***

    // No OVR functions involving memory are allowed after this.
    OVR::System::Destroy();
  
    OVR_DEBUG_STATEMENT(_CrtDumpMemoryLeaks());
    return exitCode;
}
