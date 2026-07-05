<script>
  let accounts = $state([]);
  let editingIndex = $state(-1);
  let editingAccount = $state(null);
  let message = $state('');
  let error = $state(false);

  const vendors = [
    { value: 'kimi', label: 'Kimi' },
    { value: 'codex', label: 'Codex' },
    { value: 'volcengine', label: 'Volcengine' },
    { value: 'deepseek', label: 'DeepSeek' },
    { value: 'newapi', label: 'NewApi' },
    { value: 'openai_platform', label: 'OpenAI Platform' },
  ];

  const modes = [
    { value: 'coding_plan', label: 'Coding Plan' },
    { value: 'token_plan', label: 'Token Plan' },
    { value: 'balance', label: 'Balance' },
  ];

  const authTypes = [
    { value: 'bearer', label: 'Bearer' },
    { value: 'volcengine', label: 'Volcengine' },
    { value: 'newapi', label: 'NewApi' },
    { value: 'codex_oauth', label: 'Codex OAuth' },
  ];

  function blankAccount() {
    return {
      name: '',
      vendor: 'kimi',
      mode: 'token_plan',
      timeout_secs: 30,
      base_url: '',
      auth_type: 'bearer',
      api_key: '',
      ak: '',
      sk: '',
      region: 'cn-beijing',
      access_token: '',
      user_id: '',
      account_id: '',
      _hasSecret: false,
    };
  }

  function cloneAccount(acc) {
    return { ...acc };
  }

  function hasNewSecret(acc) {
    if (acc.auth_type === 'bearer') return !!(acc.api_key && acc.api_key.trim());
    if (acc.auth_type === 'volcengine') {
      return !!(acc.ak && acc.ak.trim()) || !!(acc.sk && acc.sk.trim());
    }
    if (acc.auth_type === 'newapi' || acc.auth_type === 'codex_oauth') {
      return !!(acc.access_token && acc.access_token.trim());
    }
    return false;
  }

  function getMaskedSecret(acc) {
    if (acc.auth_type === 'bearer') return acc.api_key || '-';
    if (acc.auth_type === 'volcengine') {
      return `${acc.ak || '-'} / ${acc.sk || '-'}`;
    }
    if (acc.auth_type === 'newapi' || acc.auth_type === 'codex_oauth') {
      return acc.access_token || '-';
    }
    return '-';
  }

  async function loadConfig() {
    try {
      const r = await fetch('/api/config');
      const cfg = await r.json();
      accounts = (cfg.accounts || []).map(a => ({ ...a, _hasSecret: true }));
      message = '';
    } catch (e) {
      error = true;
      message = '加载配置失败';
    }
  }

  function buildPayload() {
    const payloadAccounts = accounts.map((acc) => {
      const base = {
        name: acc.name,
        vendor: acc.vendor,
        mode: acc.mode,
        timeout_secs: Number(acc.timeout_secs) || 30,
        auth_type: acc.auth_type,
      };
      if (acc.base_url) base.base_url = acc.base_url;

      const shouldSendSecret = !acc._hasSecret || acc._secretDirty;
      const addSecret = (key, value) => {
        if (value && value.trim()) base[key] = value.trim();
      };

      if (acc.auth_type === 'bearer') {
        if (shouldSendSecret) addSecret('api_key', acc.api_key);
      } else if (acc.auth_type === 'volcengine') {
        if (shouldSendSecret) {
          addSecret('ak', acc.ak);
          addSecret('sk', acc.sk);
        }
        if (acc.region) base.region = acc.region;
      } else if (acc.auth_type === 'newapi') {
        if (shouldSendSecret) addSecret('access_token', acc.access_token);
        if (acc.user_id) base.user_id = acc.user_id;
      } else if (acc.auth_type === 'codex_oauth') {
        if (shouldSendSecret) addSecret('access_token', acc.access_token);
        if (acc.account_id) base.account_id = acc.account_id;
      }
      return base;
    });
    return { accounts: payloadAccounts };
  }

  async function saveConfig() {
    message = '';
    try {
      const payload = buildPayload();
      const r = await fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      if (r.ok) {
        error = false;
        message = '保存成功';
        await loadConfig();
      } else {
        const data = await r.json().catch(() => ({}));
        error = true;
        message = `保存失败: ${data.error || r.statusText}`;
      }
    } catch (e) {
      error = true;
      message = '保存失败: 网络错误';
    }
  }

  function startAdd() {
    editingAccount = blankAccount();
    editingIndex = accounts.length;
  }

  function startEdit(index) {
    const acc = cloneAccount(accounts[index]);
    // Clear secret fields so the form starts empty; user entered values
    // will be detected as new secrets, empty means keep existing.
    acc.api_key = '';
    acc.ak = '';
    acc.sk = '';
    acc.access_token = '';
    editingAccount = acc;
    editingIndex = index;
  }

  function cancelEdit() {
    editingAccount = null;
    editingIndex = -1;
  }

  function validateAccount(acc, isNew) {
    if (!acc.name.trim()) return '请输入名称';
    if (!acc.vendor) return '请选择厂商';
    if (!acc.mode) return '请选择模式';
    if (!acc.auth_type) return '请选择认证类型';
    if (acc.auth_type === 'bearer' && isNew && (!acc.api_key || !acc.api_key.trim())) {
      return '请输入 API Key';
    }
    if (acc.auth_type === 'volcengine') {
      if (isNew && (!acc.ak || !acc.ak.trim())) return '请输入 AK';
      if (isNew && (!acc.sk || !acc.sk.trim())) return '请输入 SK';
    }
    if ((acc.auth_type === 'newapi' || acc.auth_type === 'codex_oauth') && isNew && (!acc.access_token || !acc.access_token.trim())) {
      return '请输入 Access Token';
    }
    return null;
  }

  function confirmEdit() {
    const isNew = editingIndex >= accounts.length;
    const err = validateAccount(editingAccount, isNew);
    if (err) {
      error = true;
      message = err;
      return;
    }
    error = false;
    message = '';

    const updatedAccount = { ...editingAccount };
    if (isNew) {
      updatedAccount._hasSecret = false;
    } else {
      updatedAccount._hasSecret = true;
      if (hasNewSecret(updatedAccount)) {
        updatedAccount._secretDirty = true;
      }
    }

    if (isNew) {
      accounts = [...accounts, updatedAccount];
    } else {
      accounts = accounts.map((a, i) => i === editingIndex ? updatedAccount : a);
    }
    editingAccount = null;
    editingIndex = -1;
    saveConfig();
  }

  function deleteAccount(index) {
    if (!confirm('确定删除这个账户？')) return;
    if (index === editingIndex) {
      editingAccount = null;
      editingIndex = -1;
    }
    accounts = accounts.filter((_, i) => i !== index);
    saveConfig();
  }

  loadConfig();
</script>

<div>
  <div class="header">
    <h2>账户配置</h2>
    <button onclick={startAdd}>+ 添加账户</button>
  </div>

  <table>
    <thead>
      <tr>
        <th>名称</th>
        <th>厂商</th>
        <th>模式</th>
        <th>密钥</th>
        <th>操作</th>
      </tr>
    </thead>
    <tbody>
      {#each accounts as acc, i}
        <tr>
          <td>{acc.name}</td>
          <td>{vendors.find(v => v.value === acc.vendor)?.label || acc.vendor}</td>
          <td>{modes.find(m => m.value === acc.mode)?.label || acc.mode}</td>
          <td class="secret">{getMaskedSecret(acc)}</td>
          <td>
            <button onclick={() => startEdit(i)}>编辑</button>
            <button onclick={() => deleteAccount(i)} class="danger">删除</button>
          </td>
        </tr>
      {:else}
        <tr>
          <td colspan="5" class="empty">暂无账户</td>
        </tr>
      {/each}
    </tbody>
  </table>

  {#if editingAccount}
    <div class="modal">
      <div class="modal-content">
        <h3>{editingIndex >= accounts.length ? '添加账户' : '编辑账户'}</h3>
        <label>
          名称
          <input bind:value={editingAccount.name} />
        </label>
        <label>
          厂商
          <select bind:value={editingAccount.vendor}>
            {#each vendors as v}
              <option value={v.value}>{v.label}</option>
            {/each}
          </select>
        </label>
        <label>
          模式
          <select bind:value={editingAccount.mode}>
            {#each modes as m}
              <option value={m.value}>{m.label}</option>
            {/each}
          </select>
        </label>
        <label>
          Base URL
          <input bind:value={editingAccount.base_url} placeholder="可选" />
        </label>
        <label>
          超时（秒）
          <input type="number" bind:value={editingAccount.timeout_secs} />
        </label>
        <label>
          认证类型
          <select bind:value={editingAccount.auth_type}>
            {#each authTypes as t}
              <option value={t.value}>{t.label}</option>
            {/each}
          </select>
        </label>

        {#if editingAccount.auth_type === 'bearer'}
          <label>
            API Key（留空保留原值）
            <input bind:value={editingAccount.api_key} />
          </label>
        {:else if editingAccount.auth_type === 'volcengine'}
          <label>
            AK（留空保留原值）
            <input bind:value={editingAccount.ak} />
          </label>
          <label>
            SK（留空保留原值）
            <input bind:value={editingAccount.sk} />
          </label>
          <label>
            Region
            <input bind:value={editingAccount.region} />
          </label>
        {:else if editingAccount.auth_type === 'newapi'}
          <label>
            Access Token（留空保留原值）
            <input bind:value={editingAccount.access_token} />
          </label>
          <label>
            User ID（可选）
            <input bind:value={editingAccount.user_id} />
          </label>
        {:else if editingAccount.auth_type === 'codex_oauth'}
          <label>
            Access Token（留空保留原值）
            <input bind:value={editingAccount.access_token} />
          </label>
          <label>
            Account ID（可选）
            <input bind:value={editingAccount.account_id} />
          </label>
        {/if}

        <div class="actions">
          <button onclick={confirmEdit}>保存</button>
          <button onclick={cancelEdit} class="secondary">取消</button>
        </div>
      </div>
    </div>
  {/if}

  {#if message}<p class:err={error} class:ok={!error}>{message}</p>{/if}
</div>

<style>
  .header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 0.5rem;
  }
  h2 {
    margin: 0;
  }
  table {
    width: 100%;
    border-collapse: collapse;
  }
  th, td {
    text-align: left;
    padding: 0.6rem 0.5rem;
    border-bottom: 1px solid #ddd;
  }
  th {
    font-weight: 600;
    background: #f5f5f5;
  }
  .secret {
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    color: #666;
  }
  .empty {
    text-align: center;
    color: #888;
    padding: 2rem;
  }
  button {
    padding: 0.5rem 1rem;
    background: #2196f3;
    color: white;
    border: none;
    border-radius: 4px;
    cursor: pointer;
  }
  button.danger {
    background: #d32f2f;
  }
  button.secondary {
    background: #888;
  }
  .modal {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: rgba(0,0,0,0.5);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 100;
  }
  .modal-content {
    background: white;
    padding: 1.5rem;
    border-radius: 8px;
    width: 90%;
    max-width: 480px;
    max-height: 90vh;
    overflow-y: auto;
  }
  label {
    display: block;
    margin-bottom: 0.75rem;
  }
  input, select {
    width: 100%;
    padding: 0.4rem;
    margin-top: 0.25rem;
    box-sizing: border-box;
  }
  .actions {
    display: flex;
    gap: 0.5rem;
    margin-top: 1rem;
  }
  .ok { color: #388e3c; }
  .err { color: #d32f2f; }
</style>
