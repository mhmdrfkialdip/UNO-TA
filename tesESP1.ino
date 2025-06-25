// ESP1: TX ke ESP2 RX (TX=17), RX dari ESP2 TX (RX=16)

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("ESP1 Siap mengirim...");
}

void loop() {
  Serial2.println("Halo dari ESP1");
  Serial.println("Mengirim data ke ESP2...");
  delay(1000);
}
