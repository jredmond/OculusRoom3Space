/************************************************************************************

Filename    :   OculusRoomTiny.h
Content     :   Simplest possible first-person view test application for Oculus Rift
Created     :   March 10, 2012
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
#ifndef INC_OculusRoomTiny_h
#define INC_OculusRoomTiny_h

#include <Windows.h>
#include <xinput.h>

#include "OVR.h"
#include "Util/Util_Render_Stereo.h"
#include "../../LibOVR/Src/Kernel/OVR_Timer.h"
#include "RenderTiny_D3D1X_Device.h"

using namespace OVR;
using namespace OVR::RenderTiny;

//-------------------------------------------------------------------------------------
// ***** OculusRoomTiny Description

// This app renders a simple flat-shaded room allowing the user to move along the 
// floor and look around with an HMD, mouse, keyboard and gamepad. 
// By default, the application will start full-screen on Oculus Rift.
// 
// The following keys work:
//
//  'W', 'S', 'A', 'D' - Move forward, back; strafe left/right.
//  F1 - No stereo, no distortion.
//  F2 - Stereo, no distortion.
//  F3 - Stereo and distortion.
//

// The world RHS coordinate system is defines as follows (as seen in perspective view):
//  Y - Up
//  Z - Back
//  X - Right
const Vector3f UpVector(0.0f, 1.0f, 0.0f);
const Vector3f ForwardVector(0.0f, 0.0f, -1.0f);
const Vector3f RightVector(1.0f, 0.0f, 0.0f);

// We start out looking in the positive Z (180 degree rotation).
const float    YawInitial  = 3.141592f;
const float    Sensitivity = 1.0f;
const float    MoveSpeed   = 3.0f; // m/s


//-------------------------------------------------------------------------------------
// ***** OculusRoomTiny Application class

// An instance of this class is created on application startup (main/WinMain).
// 
// It then works as follows:
//
//  OnStartup   - Window, graphics and HMD setup is done here.
//                This function will initialize OVR::DeviceManager and HMD,
//                creating SensorDevice and attaching it to SensorFusion.
//                This needs to be done before obtaining sensor data.
//                
//  OnIdle      - Does per-frame processing, processing SensorFusion and
//                movement input and rendering the frame.

class OculusRoomTinyApp : public MessageHandler
{
public:
    OculusRoomTinyApp(HINSTANCE hinst);
    ~OculusRoomTinyApp();

    // Initializes graphics, Rift input and creates world model.
    virtual int  OnStartup(const char* args);
    // Called per frame to sample SensorFucion and render the world.
    virtual void OnIdle();

    // Installed for Oculus device messages. Optional.
    virtual void OnMessage(const Message& msg);

    // Handle input events for movement.
    virtual void OnGamepad(float padLx, float padLY, float padRx, float padRy);  
    virtual void OnMouseMove(int x, int y, int modifiers);    
    virtual void OnKey(unsigned vk, bool down);

    // Render the view for one eye.
    void         Render(const StereoEyeParams& stereo);

    // Main application loop.
    int          Run();

    // Return amount of time passed since application started in seconds.
    double       GetAppTime() const
    {
        return (OVR::Timer::GetTicks() - StartupTicks) * (1.0 / (double)OVR::Timer::MksPerSecond);
    }


protected:
    
    // Win32 window setup interface.
    LRESULT     windowProc(UINT msg, WPARAM wp, LPARAM lp);
    bool        setupWindow();
    void        destroyWindow();
    // Win32 static function that delegates to WindowProc member function.
    static LRESULT CALLBACK systemWindowProc(HWND window, UINT msg, WPARAM wp, LPARAM lp);

    void        giveUsFocus(bool setFocus);

    static OculusRoomTinyApp*   pApp;

    // *** Rendering Variables
    Ptr<RenderDevice>   pRender;
    RendererParams      RenderParams;
    int                 Width, Height;


    // *** Win32 System Variables
    HWND                hWnd;
    HINSTANCE           hInstance;    
    POINT               WindowCenter; // In desktop coordinates
    bool                Quit;
    bool                MouseCaptured;

    // Dynamically ink to XInput to simplify projects.    
    typedef DWORD (WINAPI *PFn_XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
    PFn_XInputGetState  pXInputGetState;
    HMODULE             hXInputModule;
    UInt32              LastPadPacketNo;
   

    // *** Oculus HMD Variables
    
    Ptr<DeviceManager>  pManager;
    Ptr<SensorDevice>   pSensor;
    Ptr<HMDDevice>      pHMD;
    SensorFusion        SFusion;
    OVR::HMDInfo        HMDInfo;

    // Last update seconds, used for move speed timing.
    double              LastUpdate;
    UInt64              StartupTicks;

     // Position and look. The following apply:
    Vector3f            EyePos;
    float               EyeYaw;         // Rotation around Y, CCW positive when looking at RHS (X,Z) plane.
    float               EyePitch;       // Pitch. If sensor is plugged in, only read from sensor.
    float               EyeRoll;        // Roll, only accessible from Sensor.
    float               LastSensorYaw;  // Stores previous Yaw value from to support computing delta.

    // Movement state; different bits may be set based on the state of keys.
    UByte               MoveForward;
    UByte               MoveBack;
    UByte               MoveLeft;
    UByte               MoveRight;
    Vector3f            GamepadMove, GamepadRotate;

    Matrix4f            View;
    RenderTiny::Scene   Scene;
   
    // Stereo view parameters.
    StereoConfig        SConfig;
    PostProcessType     PostProcess;

    // Shift accelerates movement/adjustment velocity. 
    bool                ShiftDown;
    bool                ControlDown;
};

// Adds sample models and lights to the argument scene.
void PopulateRoomScene(Scene* scene, RenderDevice* render);


#endif
