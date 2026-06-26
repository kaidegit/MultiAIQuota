<script>
  let configText = $state('');
  let message = $state('');
  let error = $state(false);

  async function loadConfig() {
    try {
      const r = await fetch('/api/config');
      const cfg = await r.json();
      configText = JSON.stringify(cfg, null, 2);
      message = '';
    } catch (e) {
      error = true;
      message = '加载配置失败';
    }
  }

  async function saveConfig() {
    message = '';
    try {
      JSON.parse(configText); // 预校验
      const r = await fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: configText
      });
      if (r.ok) {
        error = false;
        message = '保存成功';
      } else {
        const data = await r.json().catch(() => ({}));
        error = true;
        message = `保存失败: ${data.error || r.statusText}`;
      }
    } catch (e) {
      error = true;
      message = 'JSON 格式错误';
    }
  }

  loadConfig();
</script>

<div>
  <h2>账户配置</h2>
  <textarea bind:value={configText} rows="18"></textarea>
  <div class="actions">
    <button onclick={saveConfig}>保存</button>
    <button onclick={loadConfig} class="secondary">刷新</button>
  </div>
  {#if message}<p class:err={error} class:ok={!error}>{message}</p>{/if}
</div>

<style>
  textarea {
    width: 100%;
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    font-size: 0.85rem;
    padding: 0.5rem;
    box-sizing: border-box;
  }
  .actions {
    display: flex;
    gap: 0.5rem;
    margin-top: 0.5rem;
  }
  button {
    padding: 0.5rem 1rem;
    background: #2196f3;
    color: white;
    border: none;
    border-radius: 4px;
    cursor: pointer;
  }
  button.secondary {
    background: #888;
  }
  .ok { color: #388e3c; }
  .err { color: #d32f2f; }
</style>
