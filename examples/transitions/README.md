# Wallpaper transition shaders

A custom wallpaper transition is a GLSL ES 3.00 fragment shader. Only the fragment
stage is supplied; the fixed vertex stage provides `v_texcoord` and expects the
attributes `pos` and `texcoord`. The output must be
`layout(location = 0) out vec4 fragColor`.

Both textures are already fit-resolved full-size frames (each with its own fit
mode), so the shader only blends them; there is no fit math to do. Progress is
**linear** `0 -> 1` over `transition_duration`; any easing is the shader's
business.

| uniform | type | required | meaning |
|---|---|---|---|
| `tex1` | sampler2D | yes | outgoing frame (sample at `v_texcoord`) |
| `tex2` | sampler2D | yes | incoming frame |
| `progress` | float | yes | linear 0..1, no easing applied |
| `alpha` | float | no | element alpha, multiply the output by it |
| `duration` | float | no | transition length in seconds |
| `fullSize` | vec2 | no | frame size in pixels |
| `seed` | vec2 | no | random point in 0..1, new per transition |

A shader that fails to compile or link is rejected (logged) and the
built-in crossfade is used instead.

## Configuration

    transition_duration = 800
    transition_shader = ~/wall/shaders/diagonal-wipe.frag

    wallpaper {
      monitor = DP-1
      path = ~/wall/foo.png
      transition_duration = 250
      transition_shader = ~/wall/shaders/circle-reveal.frag
    }

`transition_duration` is in milliseconds; `0` (the default) is an instant
swap. A block's values override the top level; an explicit empty block
`transition_shader =` selects the built-in crossfade even if a top-level
shader is set. Shader paths accept `~` and config-relative paths.

## hyprctl

    hyprctl hyprpaper wallpaper DP-1,/home/me/wall/foo.png,contain,/home/me/wall/diagonal-wipe.frag

Uses the configured top-level `transition_duration`; the shader argument
overrides `transition_shader` for that call. Without it the configured
shader (or the built-in crossfade) runs.
