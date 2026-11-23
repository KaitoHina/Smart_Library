# 在學校 Windows 電腦公開展示 Dashboard

這份流程適用於臨時專題展示。學校電腦運行 Dashboard server、唯讀轉發程式及 Cloudflare Tunnel，ESP8266 經區網上傳數據，手機經公開 HTTPS 網址查看。

```text
Mega → ESP8266 → 電腦區網 IP:8080/api/telemetry
                         ↓
手機 → Cloudflare Tunnel → 127.0.0.1:8081（唯讀）→ Dashboard server:8080
```

## 1. 首次準備

將整個專案下載並解壓到學校電腦。安裝 Node.js 22 或以上，以及 Cloudflare 官方的 Windows cloudflared。可從 [Cloudflare 下載頁](https://developers.cloudflare.com/tunnel/downloads/)下載 Windows 64-bit executable，將它命名為 `cloudflared.exe`，放在專案根目錄。

在專案資料夾的檔案總管地址欄輸入 `powershell`，按 Enter。以下指令均在專案根目錄執行。每次開新的 PowerShell 視窗，都用同一個資料夾。

```powershell
node -v
.\cloudflared.exe --version
```

若學校政策限制程式或腳本執行，需請學校 IT 協助。確認電腦有上網，展示期間保持開機並避免睡眠。

## 2. 先完成區網 Wi-Fi 收數

在 PowerShell 執行 `ipconfig`，找出 ESP8266 能連到的電腦網卡 IPv4。例如電腦地址為 `192.168.1.23`。

在 Mega sketch 配置 Wi-Fi 名稱、密碼，以及 `DASHBOARD_HOST = "192.168.1.23"`，`DASHBOARD_PORT = 8080`，並保持 `WIFI_TELEMETRY` 為 `1`。按實際電腦地址填寫。接線、AT firmware 和 library 詳情見 [Wi-Fi 接駁指引](dashboard.md#wi-fi-setup-with-esp8266)。設定完成後上傳 Mega sketch。

在第一個 PowerShell 視窗執行並保持開啟。

```powershell
.\start-dashboard-wifi.ps1
```

電腦開 `http://localhost:8080`，選 **Live hardware**。確認 **Receiving data**，並打開 **Diagnostics & settings** 確認接收時間持續更新。校園網絡需要允許 ESP8266 連到電腦的 TCP 8080，Windows 防火牆亦需容許該連線。

## 3. 啟動只容許讀取的公開入口

在第二個 PowerShell 視窗，完整複製以下整段執行。它在本機 8081 建立唯讀入口，只將網頁和最新數據的 GET 請求轉發到 8080，其他方法或路徑會被拒絕。這段程式留在終端執行即可。

```powershell
@'
const http = require('node:http');
const allowedPaths = new Set(['/', '/smart-library-weekly-dashboard.html', '/api/telemetry']);

http.createServer((request, response) => {
  const path = request.url.split('?')[0];
  if (request.method !== 'GET') {
    response.writeHead(405, { Allow: 'GET' });
    return response.end('Read-only endpoint');
  }
  if (!allowedPaths.has(path)) {
    response.writeHead(404);
    return response.end('Not found');
  }
  const upstream = http.get({
    hostname: '127.0.0.1',
    port: 8080,
    path,
    timeout: 5000
  }, incoming => {
    response.writeHead(incoming.statusCode, incoming.headers);
    incoming.pipe(response);
  });
  upstream.on('timeout', () => upstream.destroy());
  upstream.on('error', () => {
    if (!response.headersSent) response.writeHead(502);
    response.end('Dashboard server unavailable');
  });
}).listen(8081, '127.0.0.1', () => {
  console.log('Read-only dashboard ready at http://127.0.0.1:8081');
});
'@ | node
```

看到 `Read-only dashboard ready` 後，電腦瀏覽器開 `http://127.0.0.1:8081`。選 Live hardware，確認同樣能顯示感測器數據。

## 4. 開 Cloudflare Tunnel

在第三個 PowerShell 視窗執行。

```powershell
.\cloudflared.exe tunnel --url http://127.0.0.1:8081
```

終端會印出一個 `https://隨機名稱.trycloudflare.com` 網址。手機開該網址，選 **Live hardware**。可以用手機流動數據測試公開存取。

Tunnel 必須指向 **8081**，確保公網經過唯讀入口。ESP8266 繼續 POST 到電腦區網地址的 **8080**。

## 5. 驗證公開入口

在第四個 PowerShell 視窗，填入剛才產生的真實網址，再執行。

```powershell
$publicUrl = 'https://填入你的網址.trycloudflare.com'
curl.exe -i "$publicUrl/api/telemetry"
curl.exe -i -X POST "$publicUrl/api/telemetry" -H 'Content-Type: application/json' --data '{}'
```

第一個請求應收到 HTTP 200 和最新 JSON。第二個應收到 HTTP 405，代表公開入口拒絕寫入。最後在手機對照實際坐下、離座或噪音警示，確認畫面和時間更新。

## 6. 設定面板和停止展示

訪客仍可打開 Diagnostics 並查看資料。Dashboard 自訂門檻儲存在各自瀏覽器，只影響該瀏覽器的顯示。公開入口的唯讀限制保護 server 上的感測器數據。展示時各裝置選 Firmware，可以對照同一份板上判定。

結束時依次在 Cloudflare、唯讀入口、Dashboard 三個視窗按 Ctrl+C。電腦 server 或 Tunnel 停止後，手機就無法繼續取得即時資料。

Quick Tunnel 適用臨時測試，無需帳戶或網域。網址每次重開會變，服務沒有持續可用保證。日後需要固定網址時，可以建立正式 Cloudflare Tunnel。參考 [官方 Quick Tunnel 文件](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/)。

## 排錯

| 現象 | 檢查方式 |
|---|---|
| 本機 8080 顯示 Waiting for data | 先檢查 ESP8266、電腦 IPv4、firmware 設定及區網防火牆 |
| 8081 顯示 502 | 確認第一個視窗的 Dashboard server 正在 8080 運行 |
| 8081 顯示 EADDRINUSE | 已有程式使用 8081，先找回並停止之前的唯讀入口 |
| 公開網址顯示 502 | 確認第二個視窗仍在運行，Tunnel 指向 127.0.0.1:8081 |
| POST 收到 202 | Tunnel 可能指向 8080，停止並用上述 8081 指令重新啟動 |
| JSON 是 null | Server 未收到任何有效資料，先完成第二節 |
| 本機可讀，Tunnel 無法連線 | 查 cloudflared 終端錯誤，校園網絡可能限制 Tunnel 連線 |
| 手機設定跟電腦不同 | 設定保存在各自瀏覽器，重新選 Firmware 或個別調整 |

本指引已按 repo 現有資料路徑整理。學校網絡、ESP8266 和手機的整體連線需要到場驗證。
