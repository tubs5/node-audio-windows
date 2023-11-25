#pragma once
#define _WINSOCKAPI_
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <stdio.h>
#include <iostream>
#include <nan.h>
#include <wrl/client.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <atlstr.h>
using namespace std;

using Microsoft::WRL::ComPtr;

template<typename ... Args>
std::string string_format(std::string format, Args ... args) {
  size_t size = std::snprintf( nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
  std::unique_ptr<char[]> buf( new char[ size ] );
  std::snprintf( buf.get(), size, format.c_str(), args ...);
  return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}
struct DeviceDetails
{
  LPWSTR id;
  LPWSTR name;
};

void checkErrors(HRESULT hr, std::string error_message) {
  if (FAILED(hr)) {
    throw string_format("%s (0x%X)", error_message.c_str() , hr);
  }
}

class VolumeControl {
  private:
  ComPtr<IAudioEndpointVolume> device;
  UINT numberOfDevices = 0;
  struct DeviceDetails* dDetails;
  ComPtr<IMMDeviceCollection> deviceList;
  public:

  VolumeControl() {
    // The IMMDeviceEnumerator interface provides methods for enumerating multimedia device resources. Basically the interface of the requested object.
    ComPtr<IMMDeviceEnumerator> deviceEnumerator;

    // Creates a single uninitialized object of the class associated with a specified CLSID
    checkErrors(
      CoCreateInstance(
        __uuidof(MMDeviceEnumerator),   // Requested COM device enumerator id
        NULL,                           // If NULL, indicates that the object is not being created as part of an aggregate.
        CLSCTX_INPROC_SERVER,           // Context in which the code that manages the newly created object will run (Same process).
        IID_PPV_ARGS(&deviceEnumerator) // Address of pointer variable that receives the interface pointer requested in riid.
      ),
      "Error when trying to get a handle to MMDeviceEnumerator device enumerator"
    );

    checkErrors(
      deviceEnumerator->EnumAudioEndpoints(
        eRender,          // Audio rendering stream. Audio data flows from the application to the audio endpoint device, which renders the stream. eCapture would be the opposite
        DEVICE_STATE_ACTIVE,         // The role that the system has assigned to an audio endpoint device. eConsole for games, system notification sounds, and voice commands
        &deviceList   
      ),
      "Error when trying to get a handle to the default audio enpoint"
    );
    

    deviceList->GetCount(&numberOfDevices);
    dDetails = (DeviceDetails*) malloc(numberOfDevices*sizeof(DeviceDetails));
    IMMDevice* listDevice;
    LPWSTR id;
    IPropertyStore* properiyStore;
    PROPVARIANT propvariant;
    LPWSTR pwszID = NULL;
    PropVariantInit(&propvariant);
    for (int i = 0; i < numberOfDevices; i++)
    {
      DeviceDetails d;
      deviceList->Item(i,&listDevice);
      (*listDevice).GetId(&d.id);
      (*listDevice).OpenPropertyStore(STGM_READ,&properiyStore);
      properiyStore->GetValue(PKEY_Device_FriendlyName,&propvariant);
      d.name = propvariant.pwszVal;
      dDetails[i] = d;
    }

    // Device interface pointer where we will dig the audio device endpoint
    ComPtr<IMMDevice> defaultDevice;

    checkErrors(
      deviceEnumerator->GetDefaultAudioEndpoint(
        eRender,          // Audio rendering stream. Audio data flows from the application to the audio endpoint device, which renders the stream. eCapture would be the opposite
        eConsole,         // The role that the system has assigned to an audio endpoint device. eConsole for games, system notification sounds, and voice commands
        &defaultDevice    // Pointer to default audio enpoint device
      ),
      "Error when trying to get a handle to the default audio enpoint"
    );

    checkErrors(
      defaultDevice->Activate(            // Creates a COM object with the specified interface.
        __uuidof(IAudioEndpointVolume),   // Reference to a GUID that identifies the interface that the caller requests be activated
        CLSCTX_INPROC_SERVER,             // Context in which the code that manages the newly created object will run (Same process).
        NULL,                             // Set NULL to activate the IAudioEndpointVolume endpoint https://msdn.microsoft.com/en-us/library/ms679029.aspx
        &device                           //  Pointer to a pointer variable into which the method writes the address of the interface specified by parameter iid. Through this method, the caller obtains a counted reference to the interface.
      ),
      "Error when trying to get a handle to the volume endpoint"
    );
    
    
    
    

  }

  UINT getnumberOfDevices(){
    return numberOfDevices;
  }
  string getDeviceName(UINT id){
    return CW2A (dDetails[id].name);
  }
  string getDeviceId(UINT idd){
    return CW2A (dDetails[idd].id);
  }

  bool switchAudioDevice(string Inid){

    IMMDevice* listDevice;
    LPWSTR id;
    for (int i = 0; i < numberOfDevices; i++)
    {
      deviceList->Item(i,&listDevice);
      (*listDevice).GetId(&id);


      if( Inid.compare(CW2A(id)) == 0){

        
        device.Detach();

        listDevice->Activate(            // Creates a COM object with the specified interface.
        __uuidof(IAudioEndpointVolume),   // Reference to a GUID that identifies the interface that the caller requests be activated
        CLSCTX_INPROC_SERVER,             // Context in which the code that manages the newly created object will run (Same process).
        NULL,                             // Set NULL to activate the IAudioEndpointVolume endpoint https://msdn.microsoft.com/en-us/library/ms679029.aspx
        &device                           //  Pointer to a pointer variable into which the method writes the address of the interface specified by parameter iid. Through this method, the caller obtains a counted reference to the interface.
        );

        return true;
      }
    }
    return false;
  }




  BOOL isMuted() {
    BOOL muted = false;

    checkErrors(device->GetMute(&muted), "getting muted state");

    return muted;
  }

  void setMuted(BOOL muted) {
    checkErrors(device->SetMute(muted, NULL), "setting mute");
  }

  float getVolume() {
    float currentVolume = 0;

    checkErrors(
      device->GetMasterVolumeLevelScalar(&currentVolume),
      "getting volume"
    );

    return currentVolume;
  }

  void setVolume(float volume) {
    if (volume < 0.0 || volume > 1.0) {
      throw std::string("Volume needs to be between 0.0 and 1.0 inclusive");
    }

    checkErrors(
      device->SetMasterVolumeLevelScalar(volume, NULL),
      "setting volume"
    );

  }
};

class VolumeControlWrapper : public Nan::ObjectWrap {
  public:
  static NAN_MODULE_INIT(Init) {
    auto tpl = Nan::New<v8::FunctionTemplate>(New);
    tpl->SetClassName(Nan::New("VolumeControl").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(tpl, "getVolume", GetVolume);
    Nan::SetPrototypeMethod(tpl, "setVolume", SetVolume);
    Nan::SetPrototypeMethod(tpl, "isMuted", IsMuted);
    Nan::SetPrototypeMethod(tpl, "setMuted", SetMuted);
    Nan::SetPrototypeMethod(tpl, "getNumberOfDevices", getNumberOfDevices);
    Nan::SetPrototypeMethod(tpl, "getDeviceName", getDeviceName);
    Nan::SetPrototypeMethod(tpl, "getDeviceId", getDeviceId);
    Nan::SetPrototypeMethod(tpl, "switchAudioDevice", switchAudioDevice);

    constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
    Nan::Set(target, Nan::New("VolumeControl").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
  }

  private:
  VolumeControl device;

  static NAN_METHOD(New) {
    if (info.IsConstructCall()) {
      std::cout << "Constructing new object" << std::endl;
      try {
        auto obj = new VolumeControlWrapper();
        obj->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
        std::cout << "Constructed new object" << std::endl;
      } catch (std::string e) {
        return Nan::ThrowError(Nan::New(e).ToLocalChecked());
      }
    } else {
      return Nan::ThrowError(Nan::New("The constructor cannot be called as a function.").ToLocalChecked());
    }
  }

  static NAN_METHOD(getNumberOfDevices) {
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try {
      info.GetReturnValue().Set(obj->device.getnumberOfDevices());
    } catch (std::string e) {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
  }


    static NAN_METHOD(getDeviceName) {
    if (info.Length() != 1) {
      return Nan::ThrowError(Nan::New("Exactly one number parameter is required.").ToLocalChecked());
    }
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);

    int index = (int)(info[0]->NumberValue(context)).FromJust();
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try {

      v8::Local<v8::String> myString = v8::String::NewFromUtf8(isolate,obj->device.getDeviceName(index).c_str()).ToLocalChecked();

      info.GetReturnValue().Set(myString);
    } catch (std::string e) {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
    }

    static NAN_METHOD(getDeviceId) {
    if (info.Length() != 1) {
      return Nan::ThrowError(Nan::New("Exactly one number parameter is required.").ToLocalChecked());
    }
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);

    int index = (int)(info[0]->NumberValue(context)).FromJust();
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try {

      v8::Local<v8::String> myString = v8::String::NewFromUtf8(isolate,obj->device.getDeviceId(index).c_str()).ToLocalChecked();

      info.GetReturnValue().Set(myString);
    } catch (std::string e) {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
    }

    static NAN_METHOD(switchAudioDevice) {
    if (info.Length() != 1) {
      return Nan::ThrowError(Nan::New("Exactly one number parameter is required.").ToLocalChecked());
    }

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);

    v8::MaybeLocal<v8::String> maybeString = (info[0]->ToString(context));
    v8::Local<v8::String> v8String = maybeString.ToLocalChecked();
    v8::String::Utf8Value utf8Value(isolate, v8String);
    const char* utf8String = *utf8Value;
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try {

      //v8::Local<v8::String> myString = v8::String::NewFromUtf8(isolate,obj->device.getDeviceId(index).c_str()).ToLocalChecked();

      info.GetReturnValue().Set(obj->device.switchAudioDevice(utf8String));
    } catch (std::string e) {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
    }

  static NAN_METHOD(GetVolume) {
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try {
      info.GetReturnValue().Set(obj->device.getVolume());
    } catch (std::string e) {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
  }

  static NAN_METHOD(SetVolume) {
    if (info.Length() != 1) {
      return Nan::ThrowError(Nan::New("Exactly one number parameter is required.").ToLocalChecked());
    }
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    // Create a template for the global object and set the
    // built-in global functions.
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
   // global->Set(v8::String::NewFromUtf8(isolate, "log"),v8::FunctionTemplate::New(isolate, LogCallback));
  // Each processor gets its own context so different processors
  // do not affect each other.
    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);

    double volume = (double)(info[0]->NumberValue(context)).FromJust();
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try {
      obj->device.setVolume(volume);
    } catch (std::string e) {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
  }

  static NAN_METHOD(IsMuted) {
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try {
      info.GetReturnValue().Set(obj->device.isMuted());
    } catch (std::string e) {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
  }

  static NAN_METHOD(SetMuted) {
    if (info.Length() != 1) {
      return Nan::ThrowError(Nan::New("Exactly one boolean parameter is required.").ToLocalChecked());
    }
       
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    bool muted = (bool)(info[0]->BooleanValue(isolate));
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try {
      obj->device.setMuted(muted);
    } catch (std::string e) {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
  }

  static inline Nan::Persistent<v8::Function> & constructor() {
    static Nan::Persistent<v8::Function> constructorFunction;
    return constructorFunction;
  }
};

void UnInitialize(void*) {
  CoUninitialize();
}

NAN_MODULE_INIT(InitModule) {
  CoInitialize(NULL);

  VolumeControlWrapper::Init(target);

  //node::AtExit(UnInitialize);

}

NODE_MODULE(addon, InitModule)
