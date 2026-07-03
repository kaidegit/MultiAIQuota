<script>
  let src = $state('/api/screenshot?t=0');
  let auto = $state(true);

  function refresh() {
    src = `/api/screenshot?t=${Date.now()}`;
  }

  $effect(() => {
    if (!auto) return;
    const id = setInterval(refresh, 2000);
    return () => clearInterval(id);
  });
</script>

<div>
  <h2>屏幕截图</h2>
  <div class="toolbar">
    <button onclick={refresh}>刷新</button>
    <label>
      <input type="checkbox" bind:checked={auto} />
      自动刷新
    </label>
  </div>
  <img {src} alt="当前屏幕" />
</div>

<style>
  h2 { margin-top: 0; }
  .toolbar {
    display: flex;
    align-items: center;
    gap: 1rem;
    margin-bottom: 0.8rem;
  }
  button {
    padding: 0.5rem 1rem;
    background: #2196f3;
    color: white;
    border: none;
    border-radius: 4px;
    cursor: pointer;
  }
  label {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    color: #555;
  }
  img {
    display: block;
    max-width: 100%;
    image-rendering: pixelated;
    border: 1px solid #ddd;
    border-radius: 4px;
    background: #000;
  }
</style>
