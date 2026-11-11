# Character001 texture variants

- Original loop size: the 24 PNG maps in this directory.
- Double loop size: the 24 PNG maps in `Knit2x/`, using the same filenames and UVs.

Both variants contain Body, Face, Hair, Hat, Pants, Shirt, Shoes and Vest. Each
module has BaseColor, Normal_DX and ORM maps. Face is 1024 x 1024; the other
modules are 2048 x 2048.

The larger variant doubles the knit repeat dimensions in the authored surface
coordinates, with proportional yarn relief. Clothing boundaries, pockets,
buttons, tailoring overlays and UV placement retain their original positions.
Hair keeps its strand flow while its crochet detail uses larger loops.

Exported from Substance Painter with Unreal Engine (Packed):

- BaseColor: sRGB enabled.
- Normal_DX: DirectX tangent normal; import with Normalmap compression.
- ORM: R = ambient occlusion, G = roughness, B = metallic; sRGB disabled,
  Masks compression. AO and Metallic are pixel-identical between the variants.

To compare in Unreal, import `Knit2x` into a separate Content folder and use
its three maps in copies of the existing material instances. The original
textures and Unreal assets have not been replaced.

Painter sources remain local in `Source/Character001_Knit.spp` and
`Source/Character001_Knit_Loops2x.spp`. SPP files and their Painter locks are
excluded from Git.
