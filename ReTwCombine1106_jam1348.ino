#include <Keypad.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#define MAX_PRESETS 5

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {18, 8, 3, 46};
byte colPins[COLS] = {38, 37, 36, 35};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Preferences preferences;
TFT_eSPI tft = TFT_eSPI();

// Enum covering all required states from both codes
enum MenuState {
  MAIN_MENU,
  REPEATED_BENDING_MENU,
  REPEAT_INPUT_CYCLES,
  REPEAT_CONFIRM_RUN,
  REPEAT_SELECT_PRESET,
  REPEAT_EDIT_PRESET,
  REPEAT_WAITING_EXECUTION,
  REPEAT_CONFIRM_PRESET_SAVE,
  REPEAT_CONFIRM_NEW_PRESET,
  REPEAT_SHOW_RESULT,
  TWIST_TEST_MENU,
  TWIST_INPUT_LEFT,
  TWIST_INPUT_RIGHT,
  TWIST_CONFIRM_RUN,
  TWIST_SELECT_PRESET,
  TWIST_EDIT_PRESET,
  REPEAT_CONFIRM_RESET_PRESETS,
  TWIST_WAITING_EXECUTION,
  TWIST_CONFIRM_RESET_PRESETS,
  TWIST_SHOW_RESULT,
  TWIST_CONFIRM_NEW_PRESET
};

MenuState state = MAIN_MENU;

// Repeated Bending variables
float repeatPresets[MAX_PRESETS];
int RPCount = 0;
int repeatSelectedIndex = -1;
String repeatInputBuffer = "";
bool repeatManualMode = false;
float repeatManualCycles = 0;
int repeatPendingCycles = 0;
bool repeatEditingPreset = false;

// Twist Test variables
struct TwistPreset {
  float leftCycles;
  float rightCycles;
};
TwistPreset twistPresets[MAX_PRESETS];
int TPCount = 0;
int twistSelectedIndex = -1;
String twistInputBuffer = "";
bool twistInputLeft = true;
bool twistManualMode = false;
TwistPreset twistManualInput;
bool twistRunAfterConfirm = true;
float currentCycle = 0;
bool twistShowCycleProgress = false;
String serialBuffer = "";


// Function declarations (prototypes)
void loadRepeatPresets();
void loadTwistPresets();
void readSerial2();
void printTwistTestMenu();
void handleRepeatConfirmPresetSave(char key);
void handleRepeatConfirmResetPresets(char key);
void handleTwistMenu(char key);
void handleTwistInputLeft(char key);
void handleTwistInputRight(char key);
void handleTwistConfirmRun(char key);
void handleTwistSelectPreset(char key);
void handleTwistEditPreset(char key);
void handleTwistConfirmResetPresets(char key);
void handleTwistConfirmNewPreset(char key);
void showTwistInputScreen(String title, String prompt);
void showTwistFullPresetMessage();
void showTwistPresetList(bool editing);
void showTwistResetConfirmPrompt();
void showTwistNewPresetConfirmation();
void sendTwistRunCommand(float left, float right);
void handleSerialMessage(String msg);
void saveRepeatPresets();
void saveTwistPresets();
void showRepeatConfirmScreen();
void clearRepeatPresets();
void clearTwistPresets();
void showConfirmation(String msg, bool forceRedraw = false);

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  delay(1000);

  preferences.begin("combo_cfg", false);

  tft.init();
  tft.setRotation(1);
  clearTFT();

  loadRepeatPresets();
  loadTwistPresets();

  printMainMenu();
}

void loop() {
  readSerial2();

  char key = keypad.getKey();
  if (!key) return;

  if (key == '*') {
  if (state == REPEAT_WAITING_EXECUTION || state == TWIST_WAITING_EXECUTION) {
    Serial2.println("CANCEL\n");
    
    // Ubah state SEBELUM showConfirmation
    if (state == TWIST_WAITING_EXECUTION) state = TWIST_SHOW_RESULT;
    else if (state == REPEAT_WAITING_EXECUTION) state = REPEAT_SHOW_RESULT;
    
    showConfirmation("Dibatalkan oleh user.", true);
    return;
  }


    if (isRepeatedBendingSubmenu(state)) {
      state = REPEATED_BENDING_MENU;
      printRepeatBendingMenu();
      return;
    }

    if (isTwistTestSubmenu(state)) {
      state = TWIST_TEST_MENU;
      printTwistTestMenu();
      return;
    }

    resetState();
    printMainMenu();
    return;
  }

  switch (state) {
    case MAIN_MENU: handleMainMenu(key); break;
    case REPEATED_BENDING_MENU: handleRepeatMenu(key); break;
    case REPEAT_INPUT_CYCLES: handleRepeatCycleInput(key); break;
    case REPEAT_CONFIRM_NEW_PRESET: handleRepeatConfirmNewPreset(key); break;
    case REPEAT_CONFIRM_RUN: handleRepeatConfirmRun(key); break;
    case REPEAT_SELECT_PRESET: handleRepeatSelectPreset(key); break;
    case REPEAT_EDIT_PRESET: handleRepeatEditPreset(key); break;
    case REPEAT_WAITING_EXECUTION: readSerial2(); break;
    case REPEAT_CONFIRM_PRESET_SAVE: handleRepeatConfirmPresetSave(key); break;
    case REPEAT_CONFIRM_RESET_PRESETS: handleRepeatConfirmResetPresets(key); break;
    case REPEAT_SHOW_RESULT:
      if (key == '*') {
        state = REPEATED_BENDING_MENU;
        printRepeatBendingMenu();
      }
      break;
    case TWIST_TEST_MENU: handleTwistMenu(key); break;
    case TWIST_INPUT_LEFT: handleTwistInputLeft(key); break;
    case TWIST_INPUT_RIGHT: handleTwistInputRight(key); break;
    case TWIST_CONFIRM_RUN: handleTwistConfirmRun(key); break;
    case TWIST_SELECT_PRESET: handleTwistSelectPreset(key); break;
    case TWIST_EDIT_PRESET: handleTwistEditPreset(key); break;
    case TWIST_WAITING_EXECUTION: readSerial2(); break;
    case TWIST_CONFIRM_RESET_PRESETS: handleTwistConfirmResetPresets(key); break;
    case TWIST_CONFIRM_NEW_PRESET: handleTwistConfirmNewPreset(key); break;
    case TWIST_SHOW_RESULT:
      if (key == '*') {
        state = TWIST_TEST_MENU;
        printTwistTestMenu();
      }
      break;
  }
}

// Helper functions to check if the current state is within a submenu
bool isRepeatedBendingSubmenu(MenuState st) {
  switch (st) {
    case REPEAT_INPUT_CYCLES:
    case REPEAT_CONFIRM_NEW_PRESET:
    case REPEAT_CONFIRM_RUN:
    case REPEAT_SELECT_PRESET:
    case REPEAT_EDIT_PRESET:
    case REPEAT_CONFIRM_PRESET_SAVE:
    case REPEAT_WAITING_EXECUTION:
    case REPEAT_SHOW_RESULT:
    case REPEAT_CONFIRM_RESET_PRESETS:
      return true;
    default:
      return false;
  }
}

bool isTwistTestSubmenu(MenuState st) {
  switch (st) {
    case TWIST_INPUT_LEFT:
    case TWIST_INPUT_RIGHT:
    case TWIST_CONFIRM_RUN:
    case TWIST_SELECT_PRESET:
    case TWIST_EDIT_PRESET:
    case TWIST_WAITING_EXECUTION:
    case TWIST_CONFIRM_RESET_PRESETS:
    case TWIST_CONFIRM_NEW_PRESET:
    case TWIST_SHOW_RESULT:
      return true;
    default:
      return false;
  }
}

// TFT helper functions
void clearTFT() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
}

void showConfirmation(String msg, bool forceRedraw) {
  static String lastMsg = "";
  if (!forceRedraw && msg == lastMsg) return;
  lastMsg = msg;

  if (msg == "Dibatalkan oleh user.") {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
  } else {
    clearTFT();
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  }

  tft.setCursor(0, 0);
  tft.println(msg);
  tft.println();
  tft.println("* = Kembali ke menu");
}


// Resets all state variables to their default values
void resetState() {
  state = MAIN_MENU;

  repeatSelectedIndex = -1;
  repeatInputBuffer = "";
  repeatManualMode = false;
  repeatManualCycles = 0;
  repeatPendingCycles = 0;
  repeatEditingPreset = false;

  twistSelectedIndex = -1;
  twistInputBuffer = "";
  twistInputLeft = true;
  twistManualMode = false;
  twistRunAfterConfirm = true;
  twistManualInput = {0, 0};
  twistShowCycleProgress = false;
  currentCycle = 0;
  clearTFT();
}

// ---------- Main Menu ----------
void printMainMenu() {
  clearTFT();
  tft.println("=== MENU UTAMA ===");
  tft.println("1. Repeated Bending");
  tft.println("2. Twist Test");
}

void handleMainMenu(char key) {
  switch (key) {
    case '1':
      state = REPEATED_BENDING_MENU;
      printRepeatBendingMenu();
      break;
    case '2':
      state = TWIST_TEST_MENU;
      printTwistTestMenu();
      break;
  }
}

// ---------- Repeated Bending Section ----------
void printRepeatBendingMenu() {
  clearTFT();
  tft.println("=== REPEATED BENDING ===");
  tft.println("1. Repeated Bending");
  tft.println("2. Tambah Preset");
  tft.println("3. Jalankan Preset");
  tft.println("4. Edit Preset");
  tft.println("5. Reset Preset");
  tft.println("6. Posisi Idle");
}

void handleRepeatMenu(char key) {
  switch (key) {
    case '1':
      repeatManualMode = true;
      repeatInputBuffer = "";
      state = REPEAT_INPUT_CYCLES;
      showRepeatCycleInput("Repeated Bending", "Jumlah Siklus:");
      break;
    case '2':
      if (RPCount >= MAX_PRESETS) {
        tft.fillScreen(TFT_RED);
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.setCursor(0, 0);
        tft.println("Slot penuh!");
        delay(2000);
        printRepeatBendingMenu();
      } else {
        repeatSelectedIndex = -1;
        repeatManualMode = false;
        repeatEditingPreset = false;
        repeatInputBuffer = "";
        state = REPEAT_INPUT_CYCLES;
        showRepeatCycleInput("Tambah Preset", "Jumlah Siklus:");
      }
      break;
    case '3':
      state = REPEAT_SELECT_PRESET;
      showRepeatPresetList(false);
      break;
    case '4':
      state = REPEAT_EDIT_PRESET;
      showRepeatPresetList(true);
      break;
    case '5':
      state = REPEAT_CONFIRM_RESET_PRESETS;
      showRepeatResetConfirmPrompt();
      break;
    case '6':
      Serial2.print("IDLE");
      showConfirmation("Kirim IDLE ke ESP2");
      break;
  }
}

void showRepeatCycleInput(String title, String prompt) {
  clearTFT();
  tft.println(title);
  tft.println();
  tft.println(prompt);
  tft.print("Input: ");
  tft.println(repeatInputBuffer);
  tft.println();
  tft.println("*=Menu  A=Hapus  #=OK");
}

void handleRepeatCycleInput(char key) {
  if (key >= '0' && key <= '9') {
    repeatInputBuffer += key;
  } else if (key == 'A' && repeatInputBuffer.length() > 0) {
    repeatInputBuffer.remove(repeatInputBuffer.length() - 1);
  } else if (key == '#') {
    if (repeatInputBuffer.isEmpty() || repeatInputBuffer.toInt() <= 0) {
      showConfirmation("Input tidak valid!");
      return;
    }

    repeatPendingCycles = repeatInputBuffer.toInt();

    if (repeatManualMode) {
      repeatManualCycles = repeatPendingCycles;
      state = REPEAT_CONFIRM_RUN;
      showRepeatConfirmScreen();
      return;
    }

    if (repeatEditingPreset) {
      state = REPEAT_CONFIRM_PRESET_SAVE;
      showRepeatPresetSaveConfirmation();
      return;
    }

    if (RPCount >= MAX_PRESETS) {
      tft.fillScreen(TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.setCursor(0, 0);
      tft.println("Slot penuh!");
      delay(2000);
      resetState();
      printMainMenu();
      return;
    }
    state = REPEAT_CONFIRM_NEW_PRESET;
    showRepeatNewPresetConfirmation();
    return;
  }
  showRepeatCycleInput(
    repeatManualMode ? "Repeated Bending" :
    (repeatEditingPreset ? "Edit Preset" : "Tambah Preset"),
    "Jumlah Siklus:"
  );
}

void showRepeatNewPresetConfirmation() {
  clearTFT();
  tft.println("Konfirmasi Tambah:");
  tft.printf("Siklus: %d\n", repeatPendingCycles);
  tft.println("#=Simpan  *=Batal");
}

void handleRepeatConfirmNewPreset(char key) {
  if (key == '#') {
    repeatPresets[RPCount++] = repeatPendingCycles;
    saveRepeatPresets();
    clearTFT();
    tft.println("Preset disimpan");
    delay(1000);
    state = REPEATED_BENDING_MENU;
    printRepeatBendingMenu();
  } else if (key == '*') {
    state = REPEATED_BENDING_MENU;
    printRepeatBendingMenu();
  }
}

void handleRepeatConfirmRun(char key) {
  if (key == '#') {
    float cycles = repeatManualMode ? repeatManualCycles : repeatPresets[repeatSelectedIndex];
    sendRepeatRunCommand(cycles);
    state = REPEAT_WAITING_EXECUTION;
  } else if (key == '*') {
    resetState();
    printMainMenu();
  }
}

void sendRepeatRunCommand(float cycles) {
  String cmd = "RUN:" + String((int)cycles) + "\n";
  Serial2.print(cmd);
  clearTFT();
  tft.println("Menjalankan...");
  tft.printf("Siklus: %d", (int)cycles);
  tft.println();
  tft.println("*=Batal");
}

void showRepeatPresetList(bool editing) {
  clearTFT();
  tft.println(editing ? "Edit Preset" : "Pilih Preset");
  tft.println();
  for (int i = 0; i < RPCount; i++) {
    tft.printf("%d. %d siklus", i + 1, (int)repeatPresets[i]);
    tft.println();
  }
  tft.println();
  tft.println("*=Menu");
}

void handleRepeatSelectPreset(char key) {
  int idx = key - '1';
  if (idx >= 0 && idx < RPCount) {
    repeatSelectedIndex = idx;
    repeatManualMode = false;
    state = REPEAT_CONFIRM_RUN;
    showRepeatConfirmScreen();
  }
}

void handleRepeatEditPreset(char key) {
  int idx = key - '1';
  if (idx >= 0 && idx < RPCount) {
    repeatSelectedIndex = idx;
    repeatEditingPreset = true;
    repeatInputBuffer = "";
    repeatManualMode = false;
    state = REPEAT_INPUT_CYCLES;
    showRepeatCycleInput("Edit Preset", "Jumlah Siklus:");
  }
}

void showRepeatPresetSaveConfirmation() {
  clearTFT();
  tft.println("Konfirmasi Edit:");
  tft.printf("Lama: %d\n", (int)repeatPresets[repeatSelectedIndex]);
  tft.printf("Baru: %d\n", repeatPendingCycles);
  tft.println("#=Simpan  *=Batal");
}

void handleRepeatConfirmPresetSave(char key) {
  if (key == '#') {
    repeatPresets[repeatSelectedIndex] = repeatPendingCycles;
    saveRepeatPresets();

    clearTFT();
    tft.println("Preset diperbarui!");
    delay(1000);

    state = REPEATED_BENDING_MENU;
    printRepeatBendingMenu();
  } else if (key == '*') {
    resetState();
    printMainMenu();
  }
}

void showResultMessage(String msg, int originState) {
  clearTFT();
  if (msg == "Siklus selesai!") {
    tft.fillScreen(TFT_GREEN);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
  } else if (msg == "Dibatalkan oleh user.") {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  }
  tft.setCursor(0, 0);
  tft.println(msg);
  tft.println();
  tft.println("* = Kembali ke menu");

  // Gunakan originState untuk menentukan state hasil
  if (originState == TWIST_WAITING_EXECUTION) {
    state = TWIST_SHOW_RESULT;
  } else if (originState == REPEAT_WAITING_EXECUTION) {
    state = REPEAT_SHOW_RESULT;
  }
}


void clearRepeatPresets() {
  RPCount = 0;
  for (int i = 0; i < MAX_PRESETS; i++) {
    preferences.remove(("repeatPreset" + String(i)).c_str());
  }
  preferences.remove("RPCount");
  saveRepeatPresets();
}

void saveRepeatPresets() {
  preferences.putInt("RPCount", RPCount);
  for (int i = 0; i < RPCount; i++) {
    preferences.putFloat(("repeatPreset" + String(i)).c_str(), repeatPresets[i]);
  }
}

void loadRepeatPresets() {
  RPCount = preferences.getInt("RPCount", 0);
  for (int i = 0; i < RPCount; i++) {
    repeatPresets[i] = preferences.getFloat(("repeatPreset" + String(i)).c_str(), 0);
  }
}

void showRepeatConfirmScreen() {
  clearTFT();
  tft.println("Konfirmasi:");
  tft.println();
  int c = repeatManualMode ? (int)repeatManualCycles : (int)repeatPresets[repeatSelectedIndex];
  tft.printf("Siklus: %d", c);
  tft.println();
  tft.println("#=Jalankan  *=Batal");
}

void showRepeatResetConfirmPrompt() {
  clearTFT();
  tft.println("Reset semua preset?");
  tft.println();
  tft.println("#=Ya  *=Batal");
}

void handleRepeatConfirmResetPresets(char key) {
  if (key == '#') {
    clearRepeatPresets();
    clearTFT();
    tft.println("Preset direset!");
    delay(1000);
    state = REPEATED_BENDING_MENU;
    printRepeatBendingMenu();
  } else if (key == '*') {
    state = REPEATED_BENDING_MENU;
    printRepeatBendingMenu();
  }
}

// ---------- Twist Test Section ----------

void printTwistTestMenu() {
  clearTFT();
  tft.println("=== TWIST TEST ===");
  tft.println("1. Twist Manual");
  tft.println("2. Tambah Preset");
  tft.println("3. Jalankan Preset");
  tft.println("4. Edit Preset");
  tft.println("5. Reset Preset");
}

void handleTwistMenu(char key) {
  switch (key) {
    case '1':
      twistManualMode = true;
      twistInputLeft = true;
      twistInputBuffer = "";
      twistSelectedIndex = -1;
      state = TWIST_INPUT_LEFT;
      showTwistInputScreen("Twist Manual", "Masukkan KIRI:");
      break;
    case '2':
      if (TPCount >= MAX_PRESETS) {
        showTwistFullPresetMessage();
        return;
      }
      twistManualMode = false;
      twistRunAfterConfirm = false;
      twistInputLeft = true;
      twistInputBuffer = "";
      twistSelectedIndex = TPCount;
      state = TWIST_INPUT_LEFT;
      showTwistInputScreen("Tambah Preset", "Masukkan KIRI:");
      break;
    case '3':
      state = TWIST_SELECT_PRESET;
      showTwistPresetList(false);
      break;
    case '4':
      state = TWIST_EDIT_PRESET;
      showTwistPresetList(true);
      break;
    case '5':
      state = TWIST_CONFIRM_RESET_PRESETS;
      showTwistResetConfirmPrompt();
      break;
  }
}

void showTwistInputScreen(String title, String prompt) {
  clearTFT();
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(10, 10);
  tft.setTextSize(2);
  tft.println(title);
  tft.setCursor(10, 50);
  tft.println(prompt);
  tft.setCursor(10, 90);
  tft.setTextColor(TFT_WHITE);
  tft.print("Input: ");
  tft.print(twistInputBuffer);
  tft.setCursor(10, 150);
  tft.setTextColor(TFT_YELLOW);
  tft.println("A=Backspace #=OK *=Menu");
}

void handleTwistInputLeft(char key) {
  if ((key >= '0' && key <= '9') || key == 'D') {
    twistInputBuffer += (key == 'D' ? '.' : key);
  } else if (key == 'A' && twistInputBuffer.length() > 0) {
    twistInputBuffer.remove(twistInputBuffer.length() - 1);
  } else if (key == '#') {
    if (twistInputBuffer.length() == 0) {
      showConfirmation("Input tidak boleh kosong!");
      return;
    }
    float value = twistInputBuffer.toFloat();
    twistInputBuffer = "";
    if (twistManualMode) twistManualInput.leftCycles = value;
    else twistPresets[twistSelectedIndex].leftCycles = value;
    twistInputLeft = false;
    state = TWIST_INPUT_RIGHT;
    showTwistInputScreen(twistManualMode ? "Twist Manual" : (twistSelectedIndex < TPCount ? "Edit Preset" : "Tambah Preset"), "Masukkan KANAN:");
    return;
  }
  showTwistInputScreen(twistManualMode ? "Twist Manual" : (twistSelectedIndex < TPCount ? "Edit Preset" : "Tambah Preset"), "Masukkan KIRI:");
}

void handleTwistInputRight(char key) {
  if ((key >= '0' && key <= '9') || key == 'D') {
    twistInputBuffer += (key == 'D' ? '.' : key);
  } else if (key == 'A' && twistInputBuffer.length() > 0) {
    twistInputBuffer.remove(twistInputBuffer.length() - 1);
  } else if (key == '#') {
    if (twistInputBuffer.length() == 0) {
      showConfirmation("Input tidak boleh kosong!");
      return;
    }
    float value = twistInputBuffer.toFloat();
    twistInputBuffer = "";
    if (twistManualMode) {
      twistManualInput.rightCycles = value;
      state = TWIST_CONFIRM_RUN;
      showTwistConfirmScreen();
    } else {
      twistPresets[twistSelectedIndex].rightCycles = value;
      showTwistNewPresetConfirmation();
    }
    return;
  }
  showTwistInputScreen(twistManualMode ? "Twist Manual" : (twistSelectedIndex < TPCount ? "Edit Preset" : "Tambah Preset"), "Masukkan KANAN:");
}

void showTwistConfirmScreen() {
  clearTFT();
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10, 20);
  tft.setTextSize(2);
  tft.println("Konfirmasi:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 60);
  if (twistManualMode) {
    tft.printf("Kiri: %.2f", twistManualInput.leftCycles);
    tft.setCursor(10, 90);
    tft.printf("Kanan: %.2f", twistManualInput.rightCycles);
  } else {
    tft.printf("Kiri: %.2f", twistPresets[twistSelectedIndex].leftCycles);
    tft.setCursor(10, 90);
    tft.printf("Kanan: %.2f", twistPresets[twistSelectedIndex].rightCycles);
  }
  tft.setCursor(10, 130);
  tft.setTextColor(TFT_GREEN);
  tft.println("#=Jalankan");
  tft.setTextColor(TFT_RED);
  tft.println("*=Batal");
}

void handleTwistConfirmRun(char key) {
  if (key == '#') {
    if (twistManualMode) {
      // Jalankan berdasarkan input twist manual
      sendTwistRunCommand(twistManualInput.leftCycles, twistManualInput.rightCycles);
    } else {
      // Jalankan berdasarkan preset yang dipilih
      if (twistSelectedIndex >= 0 && twistSelectedIndex < TPCount) {
        sendTwistRunCommand(twistPresets[twistSelectedIndex].leftCycles, twistPresets[twistSelectedIndex].rightCycles);
      } else {
        // Jika tidak ada preset yang valid terpilih, tampilkan error atau fallback
        showConfirmation("Preset tidak valid!");
        return;
      }
    }
    state = TWIST_WAITING_EXECUTION;
  } else if (key == '*') {
    state = TWIST_TEST_MENU;
    printTwistTestMenu();
  }
}


void sendTwistRunCommand(float left, float right) {
  delay(500);
  String cmd = "RUN:" + String(left, 2) + ":" + String(right, 2) + "\n";
  Serial2.print(cmd);
  clearTFT();
  tft.println("Menjalankan...");
  tft.printf("Kiri: %.2f Kanan: %.2f", left, right);
  tft.println();
  tft.println("*=Batal");
}

void showTwistPresetList(bool editing) {
  clearTFT();
  tft.println(editing ? "Edit Preset" : "Pilih Preset");
  tft.println();
  if (TPCount == 0) {
    tft.println("Belum ada preset.");
  } else {
    for (int i = 0; i < TPCount; i++) {
      tft.printf("%d. Kiri: %.2f Kanan: %.2f", i + 1, twistPresets[i].leftCycles, twistPresets[i].rightCycles);
      tft.println();
    }
  }
  tft.println();
  tft.println("*=Menu");
}

void handleTwistSelectPreset(char key) {
  int idx = key - '1';
  if (idx >= 0 && idx < TPCount) {
    twistSelectedIndex = idx;
    twistManualMode = false;
    state = TWIST_CONFIRM_RUN;
    showTwistConfirmScreen();
  }
}

void handleTwistEditPreset(char key) {
  int idx = key - '1';
  if (idx >= 0 && idx < TPCount) {
    twistSelectedIndex = idx;
    twistInputLeft = true;
    twistManualMode = false;
    twistInputBuffer = "";
    state = TWIST_INPUT_LEFT;
    showTwistInputScreen("Edit Preset", "Masukkan KIRI:");
  }
}

void showTwistFullPresetMessage() {
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 60);
  tft.println("Preset penuh!");
  tft.setCursor(10, 90);
  tft.println("Maksimal 5 preset.");
  delay(1500);
  printTwistTestMenu();
}

void clearTwistPresets() {
  TPCount = 0;
  for (int i = 0; i < MAX_PRESETS; i++) {
    preferences.remove(("left" + String(i)).c_str());
    preferences.remove(("right" + String(i)).c_str());
  }
  preferences.remove("TPCount");
  saveTwistPresets();
}

void saveTwistPresets() {
  preferences.putInt("TPCount", TPCount);
  for (int i = 0; i < TPCount; i++) {
    preferences.putFloat(("left" + String(i)).c_str(), twistPresets[i].leftCycles);
    preferences.putFloat(("right" + String(i)).c_str(), twistPresets[i].rightCycles);
  }
}

void loadTwistPresets() {
  TPCount = preferences.getInt("TPCount", 0);
  for (int i = 0; i < TPCount; i++) {
    twistPresets[i].leftCycles = preferences.getFloat(("left" + String(i)).c_str(), 0);
    twistPresets[i].rightCycles = preferences.getFloat(("right" + String(i)).c_str(), 0);
  }
}

void readSerial2() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      serialBuffer.trim();
      handleSerialMessage(serialBuffer);
      serialBuffer = "";
    } else {
      serialBuffer += c;
    }
  }
}

void handleSerialMessage(String msg) {
  if (msg.startsWith("PROG:")) {
    clearTFT();
    tft.printf("Progress: %s", msg.c_str() + 5);
    tft.println();
    tft.println("*=Batal");
  } else if (msg == "DONE") {
    // Tambahkan parameter asal state-nya
    if (state == TWIST_WAITING_EXECUTION) {
      showResultMessage("Siklus selesai!", TWIST_WAITING_EXECUTION);
    } else if (state == REPEAT_WAITING_EXECUTION) {
      showResultMessage("Siklus selesai!", REPEAT_WAITING_EXECUTION);
    }
  } else if (msg == "CANCELLED") {
    if (state == TWIST_WAITING_EXECUTION) {
      showResultMessage("Dibatalkan oleh user.", TWIST_WAITING_EXECUTION);
    } else if (state == REPEAT_WAITING_EXECUTION) {
      showResultMessage("Dibatalkan oleh user.", REPEAT_WAITING_EXECUTION);
    }
  }
}


void showTwistResetConfirmPrompt() {
  clearTFT();
  tft.println("Reset semua preset?");
  tft.println();
  tft.println("#=Ya  *=Batal");
}

void handleTwistConfirmResetPresets(char key) {
  if (key == '#') {
    clearTwistPresets();
    clearTFT();
    tft.println("Preset direset!");
    delay(1000);
    state = TWIST_TEST_MENU;
    printTwistTestMenu();
  } else if (key == '*') {
    state = TWIST_TEST_MENU;
    printTwistTestMenu();
  }
}

void showTwistNewPresetConfirmation() {
  clearTFT();
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10, 20);
  tft.setTextSize(2);
  tft.println("Konfirmasi Tambah:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 60);
  tft.printf("Kiri: %.2f", twistPresets[twistSelectedIndex].leftCycles);
  tft.setCursor(10, 90);
  tft.printf("Kanan: %.2f", twistPresets[twistSelectedIndex].rightCycles);
  tft.setCursor(10, 130);
  tft.setTextColor(TFT_GREEN);
  tft.println("#=Simpan");
  tft.setTextColor(TFT_RED);
  tft.println("*=Batal");
  state = TWIST_CONFIRM_NEW_PRESET;
}

void handleTwistConfirmNewPreset(char key) {
  if (key == '#') {
    if (twistSelectedIndex == TPCount) TPCount++;
    saveTwistPresets();

    // Tampilkan pesan konfirmasi
    clearTFT();
    tft.println("Preset disimpan!");
    delay(1000); // Tahan pesan selama 1 detik

    state = TWIST_TEST_MENU;
    printTwistTestMenu();
  } else if (key == '*') {
    state = TWIST_TEST_MENU;
    printTwistTestMenu()  ;
  }
}
