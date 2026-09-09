const menuButton = document.querySelector('.menu-toggle');
const navigation = document.querySelector('.site-nav');

if (menuButton && navigation) {
  menuButton.addEventListener('click', () => {
    const isOpen = navigation.classList.toggle('open');
    menuButton.setAttribute('aria-expanded', String(isOpen));
    menuButton.querySelector('[aria-hidden="true"]').textContent = isOpen ? '−' : '+';
  });

  navigation.addEventListener('click', (event) => {
    if (event.target instanceof HTMLAnchorElement) {
      navigation.classList.remove('open');
      menuButton.setAttribute('aria-expanded', 'false');
      menuButton.querySelector('[aria-hidden="true"]').textContent = '+';
    }
  });
}

document.querySelectorAll('[data-copy-block]').forEach((block) => {
  const button = block.querySelector('[data-copy]');
  const code = block.querySelector('code');

  if (!button || !code) return;

  button.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(code.textContent.trim());
      button.textContent = 'Copied';
      window.setTimeout(() => { button.textContent = 'Copy'; }, 1400);
    } catch {
      button.textContent = 'Select text';
    }
  });
});
