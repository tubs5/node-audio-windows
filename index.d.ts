export class VolumeControl {
    getVolume(): number;
    setVolume(volume: number): void;
    isMuted(): boolean;
    setMuted(muted: boolean);
    getNumberOfDevices():number;
    getDeviceName(index:number): String;
    getDeviceId(index:number): String;
    switchAudioDevice(id:String) : bool;
}