/* This file is part of the dynarmic project.
 * Copyright (c) 2022 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/backend/arm64/emit_arm64.h"

#include <fmt/ostream.h>
#include <oaknut/oaknut.hpp>

#include "dynarmic/backend/arm64/a32_jitstate.h"
#include "dynarmic/backend/arm64/abi.h"
#include "dynarmic/backend/arm64/emit_context.h"
#include "dynarmic/backend/arm64/reg_alloc.h"
#include "dynarmic/ir/basic_block.h"
#include "dynarmic/ir/microinstruction.h"
#include "dynarmic/ir/opcodes.h"

namespace Dynarmic::Backend::Arm64 {

using namespace oaknut::util;

template<IR::Opcode op>
void EmitIR(oaknut::CodeGenerator&, EmitContext&, IR::Inst*) {
    ASSERT_FALSE("Unimplemented opcode {}", op);
}

template<>
void EmitIR<IR::Opcode::GetCarryFromOp>(oaknut::CodeGenerator&, EmitContext& ctx, IR::Inst* inst) {
    ASSERT(ctx.reg_alloc.IsValueLive(inst));
}

EmittedBlockInfo EmitArm64(oaknut::CodeGenerator& code, IR::Block block, const EmitConfig& emit_conf) {
    EmittedBlockInfo ebi;

    const std::vector<int> gpr_order{19, 20, 21, 22, 23, 24, 25, 26, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8};
    const std::vector<int> fpr_order{8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    RegAlloc reg_alloc{code, gpr_order, fpr_order};
    EmitContext ctx{block, reg_alloc, emit_conf, ebi};

    ebi.entry_point = code.ptr<CodePtr>();

    for (auto iter = block.begin(); iter != block.end(); ++iter) {
        IR::Inst* inst = &*iter;

        switch (inst->GetOpcode()) {
#define OPCODE(name, type, ...)                    \
    case IR::Opcode::name:                         \
        EmitIR<IR::Opcode::name>(code, ctx, inst); \
        break;
#define A32OPC(name, type, ...)                         \
    case IR::Opcode::A32##name:                         \
        EmitIR<IR::Opcode::A32##name>(code, ctx, inst); \
        break;
#define A64OPC(name, type, ...)                         \
    case IR::Opcode::A64##name:                         \
        EmitIR<IR::Opcode::A64##name>(code, ctx, inst); \
        break;
#include "dynarmic/ir/opcodes.inc"
#undef OPCODE
#undef A32OPC
#undef A64OPC
        default:
            ASSERT_FALSE("Invalid opcode: {}", inst->GetOpcode());
            break;
        }
    }

    // TODO: Add Cycles

    // TODO: Emit Terminal
    const auto term = block.GetTerminal();
    const IR::Term::LinkBlock* link_block_term = boost::get<IR::Term::LinkBlock>(&term);
    ASSERT(link_block_term);
    code.MOV(Xscratch0, link_block_term->next.Value());
    code.STUR(Xscratch0, Xstate, offsetof(A32JitState, regs) + sizeof(u32) * 15);
    ebi.relocations.emplace_back(Relocation{code.ptr<CodePtr>() - ebi.entry_point, LinkTarget::ReturnFromRunCode});
    code.NOP();

    ebi.size = code.ptr<CodePtr>() - ebi.entry_point;
    return ebi;
}

}  // namespace Dynarmic::Backend::Arm64