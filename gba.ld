ENTRY(_start)

MEMORY {
    rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 28K
    ewram (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
}

SECTIONS {
    .text : {
        KEEP(*(.crt0))
        *(.text .text.*)
        *(.glue_7 .glue_7t .vfp11_veneer .v4_bx)
        . = ALIGN(4);
    } >rom

    .posecodec : {
        __pose_code_start = .;
        KEEP(*(.posecodec))
        . = ALIGN(4);
        __pose_code_end = .;
    } >rom
    ASSERT(SIZEOF(.posecodec) <= 640, "Pose decoder overlaps small actor cache")

    .visibilityoverlay : {
        __visibility_overlay_start = .;
        KEEP(*(.visibilityoverlay))
        . = ALIGN(4);
        __visibility_overlay_end = .;
    } >rom
    ASSERT(SIZEOF(.visibilityoverlay) <= geometry_collect_leaves_end-geometry_collect_leaves,
           "Indexed collector exceeds its existing IWRAM function")

    .visibilityoverlay_direct : {
        __visibility_direct_overlay_start = .;
        KEEP(*(.visibilityoverlay_direct))
        . = ALIGN(4);
        __visibility_direct_overlay_end = .;
    } >rom
    ASSERT(SIZEOF(.visibilityoverlay_direct) <= geometry_collect_leaves_end-geometry_collect_leaves,
           "Direct indexed collector exceeds its existing IWRAM function")

    .rodata : {
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } >rom

    .ARM.exidx : { *(.ARM.exidx*) } >rom

    .iwram : {
        __iwram_start = .;
        *(.iwram .iwram.*)
        *(.data .data.*)
        . = ALIGN(4);
        __iwram_end = .;
    } >iwram AT>rom
    __iwram_lma = LOADADDR(.iwram);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } >iwram

    .ewram : {
        __ewram_start = .;
        *(.ewram .ewram.*)
        . = ALIGN(4);
        __ewram_end = .;
    } >ewram AT>rom
    __ewram_lma = LOADADDR(.ewram);

    .ewram_bss (NOLOAD) : {
        __ewram_bss_start = .;
        *(.ewram_bss .ewram_bss.*)
        . = ALIGN(4);
        __ewram_bss_end = .;
    } >ewram

    /DISCARD/ : { *(.comment) *(.ARM.attributes) *(.note*) }
}
