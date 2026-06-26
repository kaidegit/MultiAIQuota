<script>
  let status = $state({ connected: false, state: 'init', ssid: '', ip: '' });
  let ssid = $state('');
  let password = $state('');
  let message = $state('');
  let loading = $state(false);

  async function fetchStatus() {
    try {
      const r = await fetch('/api/wifi/status');
      status = await r.json();
    } catch (e) {
      status = { connected: false, state: 'error', ssid: '', ip: '' };
    }
  }

  async function connect() {
    loading = true;
    message = '';
    try {
      const r = await fetch('/api/wifi/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid, password })
      });
      const data = await r.json();
      status = data;
      message = data.success ? '已连接' : `连接失败: ${data.message || ''}`;
    } catch (e) {
      message = '请求失败';
    }
    loading = false;
  }

  async function clearCred() {
    await fetch('/api/wifi/clear', { method: 'POST' });
    await fetchStatus();
  }

  fetchStatus();
</script>

<div>
  <h2>Wi-Fi 状态</h2>
  <p><strong>状态:</strong> {status.state}</p>
  <p><strong>SSID:</strong> {status.ssid || '-'}</p>
  <p><strong>IP:</strong> {status.ip || '-'}</p>

  <h3>连接网络</h3>
  <label>
    SSID
    <input type="text" bind:value={ssid} placeholder="Wi-Fi 名称" />
  </label>
  <label>
    密码
    <input type="password" bind:value={password} placeholder="Wi-Fi 密码" />
  </label>
  <div class="actions">
    <button onclick={connect} disabled={loading || !ssid}>
      {loading ? '连接中...' : '连接'}
    </button>
    <button onclick={clearCred} class="secondary">清除凭据</button>
  </div>
  {#if message}<p class="msg">{message}</p>{/if}
</div>

<style>
  label {
    display: block;
    margin-bottom: 0.8rem;
  }
  input {
    width: 100%;
    padding: 0.5rem;
    margin-top: 0.3rem;
    box-sizing: border-box;
  }
  .actions {
    display: flex;
    gap: 0.5rem;
  }
  button {
    padding: 0.5rem 1rem;
    background: #2196f3;
    color: white;
    border: none;
    border-radius: 4px;
    cursor: pointer;
  }
  button:disabled {
    opacity: 0.6;
    cursor: not-allowed;
  }
  button.secondary {
    background: #888;
  }
  .msg {
    color: #d32f2f;
  }
</style>
