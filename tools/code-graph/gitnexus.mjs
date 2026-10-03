// Launch the locked local CLI without Windows shell argument splitting.
import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { delimiter, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const cli = fileURLToPath(new URL('./node_modules/gitnexus/dist/cli/index.js', import.meta.url));
if (!existsSync(cli)) {
  console.error('Install code graph tools first: npm ci --prefix tools/code-graph');
  process.exit(1);
}

const args = process.argv.slice(2);
// Keep maintained agent instructions and skills out of generated index updates.
if (args[0] === 'analyze' && !args.includes('--index-only')) args.push('--index-only');
const env = { ...process.env };
// Ladybug FTS needs OpenSSL 3. Reuse Git for Windows in this child only.
if (process.platform === 'win32') {
  const gitBin = join(process.env.ProgramFiles || 'C:\\Program Files', 'Git', 'mingw64', 'bin');
  if (['libcrypto-3-x64.dll', 'libssl-3-x64.dll'].every((name) => existsSync(join(gitBin, name)))) {
    const pathKey = Object.keys(env).find((key) => key.toLowerCase() === 'path') || 'PATH';
    env[pathKey] = `${gitBin}${delimiter}${env[pathKey] || ''}`;
  }
}
const child = spawn(process.execPath, [cli, ...args], {
  env,
  stdio: 'inherit',
  shell: false,
  windowsHide: true,
});
child.on('error', (error) => {
  console.error(`GitNexus could not start: ${error.message}`);
  process.exitCode = 1;
});
child.on('exit', (code) => { process.exitCode = code ?? 1; });
