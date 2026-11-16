#pragma once

enum ArtsSlot {
    ArtsBolt = 0,
    ArtsSakura = 1,
    ArtsSlotCount = 2
};

void ArtsLoad();
void ArtsTick();
void ArtsRequest(int slot);
void ArtsBeginCapture(int slot);
void ArtsCancelCapture();
bool ArtsCapturing(int slot);
void ArtsKeyLabel(int slot, char* out, int outBytes);
const char* ArtsLastStatus();
