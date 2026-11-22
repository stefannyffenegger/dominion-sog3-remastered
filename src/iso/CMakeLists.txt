# Our ISO.R32 (the game logic). Stage 1: forwards every export to the original (renamed
# ISO_orig.dll) and patches in the functions we have rewritten (ported.h).
set(ISO_ORIG_IMPLIB ${CMAKE_CURRENT_BINARY_DIR}/libiso_orig.a)
add_custom_command(
    OUTPUT ${ISO_ORIG_IMPLIB}
    COMMAND ${CMAKE_DLLTOOL} -k -d ${CMAKE_CURRENT_SOURCE_DIR}/iso_orig.def -l ${ISO_ORIG_IMPLIB}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/iso_orig.def)
add_custom_target(iso_orig_implib DEPENDS ${ISO_ORIG_IMPLIB})

add_library(iso SHARED hooks.c power.c view.c display.c loop.c screen.c unit.c action.c items.c damage.c combat.c commands.c sector.c relay.c idle.c cursor.c mouse.c recon.c records.c buttons.c popup.c click.c orders.c mapgen.c sound.c paths.c mapedit.c createmap.c scenario.c maptiles.c filelist.c tilelib.c aiglue.c sdl.c sdlexec.c sdcget.c events.c step.c netflags.c terrain.c aitasks.c aisquads.c tables.c healthbar.c radarmap.c exports.c status.c players.c intfwave.c misc.c eject.c prefs.c aibuild.c wndproc.c rubber.c plugin.c aitask1.c threatmap.c projectile.c weapons.c aiconds.c trig.c targets.c selmode.c orders2.c lineofsight.c walls.c vecscore.c aim.c movement.c pathsmooth.c vision.c shade.c aibase.c clut.c groups.c debugtime.c pathtable.c aislots.c unitorder.c unitinfo.c playerinfo.c buildinfo.c mining.c buildqueue.c buildtick.c wells.c sectors.c unitlists.c spriteinit.c engineinit.c engineexp.c unitcreate.c unitdelete.c objects.c objshow.c dispexp.c tilepos.c mainloop.c playerstate.c unittype.c unitai.c groupmove.c waypoints.c unitmove.c waypath.c pathstart.c unitface.c unitfind.c patrol.c spot.c carry.c squadlist.c squadnote.c squadtarget.c squaddefend.c unitthink.c pathinit.c pathfind.c pfhelp.c dlist.c unitcell.c patrolstep.c patrollegs.c legbox.c patroltick.c patrolend.c wallcross.c patrolblock.c patrolcells.c patrolveh.c patrolactive.c patrolcell.c patrolsoldier.c soldierway.c soldiercheck.c patroldraw.c wallunits.c unitblocked.c unitrel.c popupinit.c popupconf.c popupshow.c popupitem.c pathlink.c pathappend.c pathrelease.c debugmarks.c unitstate.c vehstate.c unitrange.c unitfacing.c unitscan.c unitattack.c unitstand.c unitheal.c unitkeep.c unittarget.c unitsettarget.c ringinit.c ringbuild.c ringcell.c ringstart.c ringfind.c ringtest.c aicommand.c aidata.c statelog.c iso.def ${PROJECT_SOURCE_DIR}/src/common/minicrt.c)
add_dependencies(iso iso_orig_implib rtlib32_implib)
set_target_properties(iso PROPERTIES PREFIX "" OUTPUT_NAME "ISO" SUFFIX ".R32")
target_compile_options(iso PRIVATE ${DOM_MODULE_FLAGS})
# away from the original's preferred address (0x10000000) and from our Rtlib32.dll
target_link_options(iso PRIVATE ${DOM_MODULE_LINK} -Wl,--image-base,0x61000000)
target_link_libraries(iso PRIVATE rtlib32 ${ISO_ORIG_IMPLIB} kernel32 user32 gdi32 winmm gcc)
