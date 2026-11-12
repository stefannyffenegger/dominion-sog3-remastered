# Our Rtlib32.dll: the whole engine, rewritten. The original (installed as
# Rtlib32_orig.dll) is only read as data (main.c).
add_library(rtlib SHARED main.c res.c res_batch.c utils.c compress.c codec.c audio.c audio_play.c audio_wave.c audio_objs.c audio_files.c audio_cd.c audio_midi.c midi_prepare.c audio_mix.c resample.c audio_stream.c display.c sprite.c view.c queue.c spritelib.c draw.c blit.c tiles.c tileblit.c rt_bg.c rt_script.c rt_input.c rt_fade.c rt_obj.c rt_plugin.c rt_cmdargs.c rt_cmds.c rt_ini.c rt_exports.c rt_startup.c rt_service.c rt_joystick.c rt_scene.c rt_interp.c rt_launch.c rt_text.c rt_timer.c rt_transfer.c rt_msg.c rt_wndproc.c rt_wallpaper.c rt_print.c rt_geom.c rt_movie.c rt_builtin_misc.c rt_controller.c rt_builtins.c crt.c math87.c rtlib32.def ${PROJECT_SOURCE_DIR}/src/common/minicrt.c)
set_target_properties(rtlib PROPERTIES PREFIX "" OUTPUT_NAME "Rtlib32" SUFFIX ".dll")
target_compile_options(rtlib PRIVATE ${DOM_MODULE_FLAGS})
# away from the original's preferred address (0x10000000)
target_link_options(rtlib PRIVATE ${DOM_MODULE_LINK} -Wl,--image-base,0x60000000)
target_link_libraries(rtlib PRIVATE kernel32 user32 gdi32 advapi32 shell32 winspool winmm ddraw dxguid gcc)
