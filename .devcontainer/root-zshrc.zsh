# PX4 devcontainer zsh settings.
# Prefer Oh My Zsh, but keep a robust fallback when unavailable.

export PATH="$HOME/.local/bin:$PATH"

# History behavior remains useful in both OMZ and fallback mode.
setopt HIST_IGNORE_DUPS HIST_REDUCE_BLANKS SHARE_HISTORY INTERACTIVE_COMMENTS
HISTSIZE=5000
SAVEHIST=5000

export ZSH="$HOME/.oh-my-zsh"
ZSH_THEME="robbyrussell"
plugins=(git zsh-autosuggestions zsh-syntax-highlighting)

if [ -s "$ZSH/oh-my-zsh.sh" ]; then
	source "$ZSH/oh-my-zsh.sh"
else
	# Fallback prompt if OMZ is not installed yet.
	autoload -Uz compinit colors vcs_info
	colors
	compinit -d "$HOME/.zcompdump"
	zstyle ':vcs_info:*' enable git
	zstyle ':vcs_info:git:*' formats ' %F{177}(%b)%f'
	precmd() { vcs_info; }
	setopt PROMPT_SUBST
	PROMPT=$'%F{81}%n%f@%F{39}%m%f %F{110}%~%f${vcs_info_msg_0_}\n%F{45}%#%f '
	RPROMPT='%F{244}%D{%H:%M}%f'
fi
