<script>
  let tab = $state('wifi');

  import WifiPage from './lib/WifiPage.svelte';
  import ConfigPage from './lib/ConfigPage.svelte';
  import QueryPage from './lib/QueryPage.svelte';
</script>

<main>
  <h1>MultiAIQuota Monitor</h1>
  <nav>
    <button class:active={tab === 'wifi'} onclick={() => tab = 'wifi'}>Wi-Fi</button>
    <button class:active={tab === 'config'} onclick={() => tab = 'config'}>账户配置</button>
    <button class:active={tab === 'query'} onclick={() => tab = 'query'}>查询</button>
  </nav>

  <section>
    {#if tab === 'wifi'}
      <WifiPage />
    {:else if tab === 'config'}
      <ConfigPage />
    {:else}
      <QueryPage />
    {/if}
  </section>
</main>

<style>
  :global(body) {
    font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    margin: 0;
    background: #f5f5f5;
  }
  main {
    max-width: 720px;
    margin: 0 auto;
    padding: 1rem;
  }
  h1 {
    text-align: center;
    color: #333;
  }
  nav {
    display: flex;
    gap: 0.5rem;
    margin-bottom: 1rem;
  }
  button {
    flex: 1;
    padding: 0.6rem;
    border: none;
    border-radius: 6px;
    background: #ddd;
    cursor: pointer;
  }
  button.active {
    background: #2196f3;
    color: white;
  }
  section {
    background: white;
    padding: 1rem;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  }
</style>
