# Claude Configuration Directory

This directory contains Claude Code configuration files for the cTrader Zorro Plugin project.

## Structure

- **`commands/`** - Custom slash commands
  - `/build` - Build the plugin
  - `/restore-working` - Restore the previous working version

- **`prompts/`** - Reusable prompt templates
  - `debug-websocket.md` - WebSocket debugging helper

## Usage

### Slash Commands
In Claude Code, you can use:
```
/build
```
to execute the build command defined in `commands/build.md`

### Prompts
Reference prompts in conversations with @-mentions or by asking Claude to use them.

## Adding New Commands

Create a new `.md` file in `commands/` directory:
```bash
.claude/commands/my-command.md
```

Then use it with `/my-command`

## References
- [Claude Code Documentation](https://docs.claude.com/claude-code)
