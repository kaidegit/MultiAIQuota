<script>
  let file = $state(null);
  let uploading = $state(false);
  let message = $state('');
  let ok = $state(false);

  function onFile(e) {
    file = e.target.files[0] || null;
    message = '';
    ok = false;
  }

  async function upload() {
    if (!file) return;
    uploading = true;
    message = '';
    ok = false;
    try {
      const r = await fetch('/api/ota', { method: 'POST', body: file });
      const data = await r.json();
      if (r.ok && data.success) {
        ok = true;
        message = '固件已写入，设备即将重启，请稍后刷新页面';
      } else {
        message = `升级失败: ${data.error || r.status}`;
      }
    } catch (e) {
      message = '请求失败（设备可能已开始重启）';
      ok = true;
    }
    uploading = false;
  }
</script>

<div>
  <h2>固件升级</h2>
  <p>选择压缩固件（<code>.bin.xz.packed</code>，由 <code>idf.py gen_compressed_ota</code> 生成），上传后设备自动重启生效。</p>
  <label>
    固件文件
    <input type="file" accept=".packed,.xz,.bin" onchange={onFile} />
  </label>
  <div class="actions">
    <button onclick={upload} disabled={uploading || !file}>
      {uploading ? '上传中...' : '上传并升级'}
    </button>
  </div>
  {#if message}<p class:msg class:success={ok}>{message}</p>{/if}
</div>

<style>
  label {
    display: block;
    margin-bottom: 0.8rem;
  }
  input[type='file'] {
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
  p.msg {
    color: #d32f2f;
  }
  p.msg.success {
    color: #2e7d32;
  }
</style>
