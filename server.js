require('dotenv').config();
const express = require('express');
const cors    = require('cors');
const midtransClient = require('midtrans-client');
const crypto  = require('crypto');

const app  = express();
app.use(cors());
app.use(express.json());

const transactions = {};

const coreApi = new midtransClient.CoreApi({
  isProduction: false,
  serverKey: process.env.MIDTRANS_SERVER_KEY,
  clientKey: process.env.MIDTRANS_CLIENT_KEY,
});

console.log("SERVER KEY:", process.env.MIDTRANS_SERVER_KEY);
console.log("CLIENT KEY:", process.env.MIDTRANS_CLIENT_KEY);

// 1. BUAT TRANSAKSI QRIS
app.post('/api/payment/create', async (req, res) => {
  const { volume, price } = req.body;
  if (!volume || !price) {
    return res.status(400).json({ error: 'volume dan price wajib diisi' });
  }

  const txId = 'HYDROPAY-' + Date.now();

  try {
    const chargeResponse = await coreApi.charge({
  payment_type: 'qris',
  transaction_details: {
    order_id: txId,
    gross_amount: price,
  },
  qris: {
    acquirer: 'gopay',
  },
  custom_field1: String(volume),
});

console.log(chargeResponse);
    transactions[txId] = {
      status:    'PENDING',
      volume:    volume,
      price:     price,
      qr_string: chargeResponse.qr_string,
      actions:   chargeResponse.actions,
    };

    res.json({
      transaction_id: txId,
      qr_string:      chargeResponse.qr_string,
      qr_url: chargeResponse.actions?.find(a => a.name === 'generate-qr-code')?.url || null,
    });

  } catch (err) {
    console.error('Midtrans error:', err);
    res.status(500).json({ error: 'Gagal membuat transaksi', detail: err.message });
  }
});

// 2. CEK STATUS
app.get('/api/payment/status/:txId', async (req, res) => {
  try {
    const statusResponse = await coreApi.transaction.status(req.params.txId);

    let status = 'PENDING';

    if (
      statusResponse.transaction_status === 'settlement' ||
      statusResponse.transaction_status === 'capture'
    ) {
      status = 'PAID';
    }

    if (
      statusResponse.transaction_status === 'expire' ||
      statusResponse.transaction_status === 'cancel' ||
      statusResponse.transaction_status === 'deny'
    ) {
      status = 'FAILED';
    }

    res.json({
      status,
      midtrans_status: statusResponse.transaction_status
    });

  } catch (err) {
    console.error('Status error:', err);
    res.status(500).json({
      error: 'Gagal cek status'
    });
  }
});

// 3. WEBHOOK DARI MIDTRANS
app.post('/api/midtrans/webhook', (req, res) => {
  const notif = req.body;

  const hash = crypto
    .createHash('sha512')
    .update(
      notif.order_id +
      notif.status_code +
      notif.gross_amount +
      process.env.MIDTRANS_SERVER_KEY
    )
    .digest('hex');

  if (hash !== notif.signature_key) {
    console.warn('Signature tidak valid!');
    return res.status(403).json({ error: 'Invalid signature' });
  }

  const txId = notif.order_id;
  if (transactions[txId]) {
    if (notif.transaction_status === 'settlement' || notif.transaction_status === 'capture') {
      transactions[txId].status = 'PAID';
      console.log(`✅ Transaksi ${txId} PAID`);
    } else if (
      notif.transaction_status === 'expire' ||
      notif.transaction_status === 'cancel' ||
      notif.transaction_status === 'deny'
    ) {
      transactions[txId].status = 'FAILED';
      console.log(`❌ Transaksi ${txId} FAILED`);
    }
  }

  res.status(200).json({ ok: true });
});

// 4. UPDATE PROGRESS DISPENSING (dari ESP32)
app.post('/api/dispense/update', (req, res) => {
  const { txId, filled, status } = req.body;
  if (transactions[txId]) {
    transactions[txId].filled = filled;
    transactions[txId].dispenseStatus = status;
  }
  res.json({ ok: true });
});

// 5. GET PROGRESS DISPENSING (dari frontend)
app.get('/api/dispense/progress/:txId', (req, res) => {
  const tx = transactions[req.params.txId];
  if (!tx) return res.status(404).json({ error: 'Tidak ditemukan' });
  res.json({
    filled: tx.filled || 0,
    status: tx.dispenseStatus || 'DISPENSING',
  });
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`HydroPay backend running at http://localhost:${PORT}`);
});