<script>
  let results = $state([]);
  let loading = $state(false);
  let message = $state('');

  async function query() {
    loading = true;
    message = '';
    try {
      const r = await fetch('/api/query', { method: 'POST' });
      if (r.ok) {
        results = await r.json();
        message = results.length === 0 ? '没有账户' : '';
      } else {
        const data = await r.json().catch(() => ({}));
        message = `查询失败: ${data.error || r.statusText}`;
        results = [];
      }
    } catch (e) {
      message = '请求失败';
      results = [];
    }
    loading = false;
  }
</script>

<div>
  <h2>额度查询</h2>
  <button onclick={query} disabled={loading}>
    {loading ? '查询中...' : '立即查询'}
  </button>
  {#if message}<p class="msg">{message}</p>{/if}

  {#each results as r}
    <div class="card">
      <h3>{r.name} <small>({r.vendor} / {r.mode})</small></h3>
      {#if !r.valid}
        <p class="err">错误: {r.error}</p>
      {:else}
        <table>
          <thead>
            <tr><th>项目</th><th>已用</th><th>剩余</th><th>总量</th><th>单位</th></tr>
          </thead>
          <tbody>
            {#each r.items as item}
              <tr>
                <td>{item.name}</td>
                <td>{item.used ?? '-'}</td>
                <td>{item.remaining ?? '-'}</td>
                <td>{item.total ?? '-'}</td>
                <td>{item.unit}</td>
              </tr>
            {/each}
          </tbody>
        </table>
      {/if}
    </div>
  {/each}
</div>

<style>
  button {
    padding: 0.5rem 1rem;
    background: #2196f3;
    color: white;
    border: none;
    border-radius: 4px;
    cursor: pointer;
  }
  button:disabled { opacity: 0.6; }
  .card {
    margin-top: 1rem;
    padding: 0.8rem;
    border: 1px solid #ddd;
    border-radius: 6px;
  }
  h3 { margin-top: 0; }
  small { font-weight: normal; color: #666; }
  table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.85rem;
  }
  th, td {
    border: 1px solid #eee;
    padding: 0.4rem;
    text-align: left;
  }
  th { background: #f0f0f0; }
  .err { color: #d32f2f; }
  .msg { color: #666; }
</style>
