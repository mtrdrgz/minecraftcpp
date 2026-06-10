// Ground truth for ClientboundContainerSetContentPacket.STREAM_CODEC.
// ClientboundContainerSetContentPacket.java:11-23 — record (int containerId, int stateId,
//   List<ItemStack> items, ItemStack carriedItem). STREAM_CODEC = StreamCodec.composite(
//     ByteBufCodecs.CONTAINER_ID,             containerId,
//     ByteBufCodecs.VAR_INT,                  stateId,
//     ItemStack.OPTIONAL_LIST_STREAM_CODEC,   items,
//     ItemStack.OPTIONAL_STREAM_CODEC,        carriedItem, ...)
// Wire order / framing (verbatim from src):
//   - CONTAINER_ID  = FriendlyByteBuf.writeContainerId = VarInt.write  (FriendlyByteBuf.java:679)
//   - VAR_INT       = plain VarInt
//   - OPTIONAL_LIST_STREAM_CODEC (ItemStack.java:147-149) = ByteBufCodecs.collection over
//       OPTIONAL_STREAM_CODEC: writeCount = VarInt(size) (ByteBufCodecs.java:399-405,428-434),
//       then each element via OPTIONAL_STREAM_CODEC.encode.
//   - OPTIONAL_STREAM_CODEC (ItemStack.java:170-194): empty -> writeVarInt(0); else
//       writeVarInt(count) + Item.STREAM_CODEC(holderRegistry ITEM -> plain VarInt(getId))
//       + DataComponentPatch.encode(EMPTY) = writeVarInt(0)added + writeVarInt(0)removed.
//   So no-component element X count N = VarInt(N) VarInt(itemId) VarInt(0) VarInt(0); empty = VarInt(0).
// This gate covers the EMPTY + NO-COMPONENT ItemStack domain only (freshly-built stacks have an
// empty component patch by construction); component-bearing stacks need the per-component patch
// codecs (a separate later wave) and are intentionally excluded.
//
//   tools/run_groundtruth.ps1 -Tool PktContainerSetContentParity -Out mcpp/build/pkt_container_set_content.tsv
//
// Row: ENC \t <containerId> \t <stateId> \t <listCount> \t <perItemSpec> \t <carriedSpec> \t <readableBytes> \t <wireHex>
//   perItemSpec  = ';'-joined elements, each "name|count|empty" (name ns:path or "-" when empty), "" if list empty
//   carriedSpec  = "name|count|empty"

import io.netty.buffer.Unpooled;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundContainerSetContentPacket;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;

public class PktContainerSetContentParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x));
        return s.toString();
    }

    static String spec(ItemStack s) {
        if (s.isEmpty()) return "-|0|1";
        String name = BuiltInRegistries.ITEM.getKey(s.getItem()).toString();
        return name + "|" + s.getCount() + "|0";
    }

    static void emit(RegistryAccess access, int containerId, int stateId, List<ItemStack> items, ItemStack carried) {
        ClientboundContainerSetContentPacket pkt =
            new ClientboundContainerSetContentPacket(containerId, stateId, items, carried);
        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
        ClientboundContainerSetContentPacket.STREAM_CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        byte[] bytes = new byte[readable];
        buf.duplicate().readBytes(bytes);
        // sanity: round-trip decode through the SAME codec
        ClientboundContainerSetContentPacket back =
            ClientboundContainerSetContentPacket.STREAM_CODEC.decode(buf);
        if (back.containerId() != containerId || back.stateId() != stateId
                || back.items().size() != items.size()
                || back.carriedItem().isEmpty() != carried.isEmpty()
                || (!carried.isEmpty() && back.carriedItem().getCount() != carried.getCount()))
            throw new IllegalStateException("round-trip mismatch");
        for (int i = 0; i < items.size(); i++) {
            ItemStack a = items.get(i), b = back.items().get(i);
            if (a.isEmpty() != b.isEmpty() || (!a.isEmpty() && a.getCount() != b.getCount()))
                throw new IllegalStateException("round-trip item mismatch @" + i);
        }
        StringBuilder per = new StringBuilder();
        for (int i = 0; i < items.size(); i++) {
            if (i > 0) per.append(';');
            per.append(spec(items.get(i)));
        }
        O.println("ENC\t" + containerId + "\t" + stateId + "\t" + items.size()
                + "\t" + per + "\t" + spec(carried) + "\t" + readable + "\t" + hex(bytes));
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // Bind item data components (registry holders start unbound; same as RegistryDataCollector
        // at runtime) — required before ItemStack.components / the OPTIONAL_STREAM_CODEC encode.
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());
        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // Item palette in registry id order (so the C++ resolves the same ns:path -> id).
        int[] ids = { 0, 1, 2, 50, 126, 127, 128, 255, 256 };
        int n = BuiltInRegistries.ITEM.size();
        List<ItemStack> palette = new ArrayList<>();
        for (int id : ids) {
            if (id >= n) continue;
            palette.add(new ItemStack(BuiltInRegistries.ITEM.byId(id), 1));
        }
        Item firstItem = BuiltInRegistries.ITEM.byId(0);
        Item secondItem = BuiltInRegistries.ITEM.byId(Math.min(1, n - 1));

        // --- containerId / stateId crossing the VarInt 1->2-byte boundary, empty list + empty carried ---
        int[] cids   = { 0, 1, 127, 128, 255 };
        int[] states = { 0, 127, 128, 16384 };
        for (int cid : cids)
            for (int st : states)
                emit(access, cid, st, new ArrayList<>(), ItemStack.EMPTY);

        // --- empty list, non-empty carried ---
        emit(access, 1, 5, new ArrayList<>(), new ItemStack(firstItem, 64));
        emit(access, 2, 5, new ArrayList<>(), new ItemStack(secondItem, 99));

        // --- list of all-empty slots (a freshly opened container is all ItemStack.EMPTY) ---
        for (int sz : new int[]{ 1, 5, 27, 36, 46 }) {
            List<ItemStack> items = new ArrayList<>();
            for (int i = 0; i < sz; i++) items.add(ItemStack.EMPTY);
            emit(access, 0, 1, items, ItemStack.EMPTY);
        }

        // --- list mixing empty + no-component stacks across item ids and counts ---
        int[] counts = { 1, 16, 64, 99, 127, 128 };
        {
            List<ItemStack> items = new ArrayList<>();
            int ci = 0;
            for (ItemStack p : palette) {
                items.add(ItemStack.EMPTY);
                items.add(new ItemStack(p.getItem(), counts[ci % counts.length]));
                ci++;
            }
            emit(access, 3, 200, items, new ItemStack(firstItem, 1));
        }

        // --- a full hotbar/inventory-ish list, carried no-component stack crossing 2-byte id ---
        {
            List<ItemStack> items = new ArrayList<>();
            for (int i = 0; i < 46; i++) {
                ItemStack p = palette.get(i % palette.size());
                if (i % 3 == 0) items.add(ItemStack.EMPTY);
                else items.add(new ItemStack(p.getItem(), counts[i % counts.length]));
            }
            ItemStack carried = palette.get(palette.size() - 1).copy();
            carried.setCount(64);
            emit(access, 200, 32767, items, carried);
        }

        O.flush();
    }
}
