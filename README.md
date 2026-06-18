# HydroPay Backend

Ini adalah backend server berbasis Node.js & Express untuk sistem HydroPay (Mesin Dispenser Air otomatis dengan pembayaran QRIS Midtrans).

## 🛠️ Persyaratan
- **Node.js** (versi 14 ke atas disarankan)
- **NPM** (Node Package Manager)
- Akun **Midtrans** (Sandbox/Production)

## 📦 Cara Instalasi

1. **Install Dependencies**
   Buka terminal di folder ini dan jalankan perintah berikut untuk mengunduh semua modul yang dibutuhkan (Express, Cors, Midtrans-Client, dll):
   ```bash
   npm install
   ```

2. **Pengaturan Environment Variables**
   Pastikan Anda memiliki file `.env` di dalam folder utama proyek ini dengan format berikut:
   ```env
   PORT=3000
   MIDTRANS_SERVER_KEY=SB-Mid-server-xxxxxxxxxxxxxxxxxxxx
   MIDTRANS_CLIENT_KEY=SB-Mid-client-xxxxxxxxxxxxxxxxxxxx
   ```
   *(Silakan ganti nilai key di atas dengan Server Key dan Client Key milik Anda dari dashboard Midtrans Sandbox).*

---

## 🚀 Cara Menjalankan Server

### Opsi 1: Menjalankan Murni di Lokal (Untuk Test Frontend Saja)
Jika Anda hanya ingin mengetes sambungan antara Frontend dan Backend di laptop yang sama (tanpa melibatkan ESP32):
```bash
node server.js
```
Server akan menyala dan bisa diakses di `http://localhost:3000`.

### Opsi 2: Menjalankan untuk Terhubung dengan ESP32 (Sangat Disarankan)
Karena backend ini dijalankan di lingkungan **WSL (Windows Subsystem for Linux)**, perangkat eksternal seperti ESP32 tidak bisa mengakses IP lokal Anda secara langsung karena terhalang oleh Firewall Windows dan isolasi jaringan WSL.

Untuk mengatasi ini, sangat disarankan menggunakan **Ngrok**:

1. **Install Ngrok:** Jika Anda belum memiliki Ngrok, ikuti panduan instalasinya di [Situs Resmi Ngrok](https://dashboard.ngrok.com/get-started/setup/windows) dan pastikan Anda sudah memasukkan Authtoken milik Anda.
2. Pastikan Anda sudah menjalankan backend di terminal WSL:
   ```bash
   node server.js
   ```
3. Buka tab terminal WSL yang baru (biarkan server tetap menyala), lalu jalankan perintah Ngrok:
   ```bash
   ngrok http 3000
   ```
   *(Catatan: Anda juga bisa menggunakan `npx ngrok http 3000` jika belum terinstal secara global).*
4. Ngrok akan menghasilkan sebuah URL publik sementara (contoh: `https://abcd-123.ngrok-free.app`).
5. **Copy URL tersebut** dan masukkan ke dalam kode `hydropay.ino` di ESP32 pada variabel `backendUrl`.
6. Sekarang ESP32 Anda bisa berkomunikasi dengan backend menembus jaringan lokal tanpa masalah!
