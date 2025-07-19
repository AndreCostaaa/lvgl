.text
.global lv_color_16_16_mix_8_asm
.type lv_color_16_16_mix_8_asm, %function

// Function: lv_color_16_16_mix_8_asm
// Arguments (following ARM64 AAPCS):
//   v0 = c1_vec (uint16x8_t)
//   v1 = c2_vec (uint16x8_t) 
//   v2 = mix (uint16x8_t)
// Returns: v0 = result (uint16x8_t)
lv_color_16_16_mix_8_asm:
    // Create comparison masks
    movi v16.8h, #0                     // zero vector for comparison
    cmeq v3.8h, v2.8h, v16.8h          // mix_zero_mask = (mix == 0)
    
    movi v16.8h, #255                   // 255 vector for comparison  
    cmeq v4.8h, v2.8h, v16.8h          // mix_full_mask = (mix == 255)
    
    cmeq v5.8h, v0.8h, v1.8h           // equal_mask = (c1_vec == c2_vec)
    
    // Adjust mix: mix = (mix + 4) >> 3
    movi v16.8h, #4                     // constant 4
    add v2.8h, v2.8h, v16.8h           // mix + 4
    ushr v2.8h, v2.8h, #3              // >> 3
    
    // Split vectors into low and high 32-bit parts
    uxtl v6.4s, v0.4h                  // c1_low = extend low half of c1_vec
    uxtl2 v7.4s, v0.8h                 // c1_high = extend high half of c1_vec
    uxtl v8.4s, v1.4h                  // c2_low = extend low half of c2_vec
    uxtl2 v9.4s, v1.8h                 // c2_high = extend high half of c2_vec
    
    // Create fg vectors: fg = c1 | (c1 << 16)
    shl v18.4s, v6.4s, #16             // temp = c1_low << 16
    orr v10.16b, v6.16b, v18.16b       // fg_low = c1_low | temp
    shl v18.4s, v7.4s, #16             // temp = c1_high << 16
    orr v11.16b, v7.16b, v18.16b       // fg_high = c1_high | temp
    
    // Create bg vectors: bg = c2 | (c2 << 16)  
    shl v18.4s, v8.4s, #16             // temp = c2_low << 16
    orr v12.16b, v8.16b, v18.16b       // bg_low = c2_low | temp
    shl v18.4s, v9.4s, #16             // temp = c2_high << 16
    orr v13.16b, v9.16b, v18.16b       // bg_high = c2_high | temp
    
    // Apply RGB mask 0x7E0F81F - load as separate constants
    mov w0, #0xF81F                    // Low part of mask
    movk w0, #0x07E0, lsl #16          // High part of mask  
    dup v16.4s, w0                     // Broadcast to vector
    and v10.16b, v10.16b, v16.16b      // fg_low &= mask
    and v11.16b, v11.16b, v16.16b      // fg_high &= mask
    and v12.16b, v12.16b, v16.16b      // bg_low &= mask
    and v13.16b, v13.16b, v16.16b      // bg_high &= mask
    
    // Extend mix to 32-bit
    uxtl v14.4s, v2.4h                 // mix_low = extend low half of mix
    uxtl2 v15.4s, v2.8h                // mix_high = extend high half of mix
    
    // Perform blend calculation: ((fg - bg) * mix) >> 5 + bg
    sub v16.4s, v10.4s, v12.4s         // diff_low = fg_low - bg_low
    sub v17.4s, v11.4s, v13.4s         // diff_high = fg_high - bg_high
    
    mul v16.4s, v16.4s, v14.4s         // scaled_low = diff_low * mix_low
    mul v17.4s, v17.4s, v15.4s         // scaled_high = diff_high * mix_high
    
    ushr v16.4s, v16.4s, #5            // shifted_low = scaled_low >> 5
    ushr v17.4s, v17.4s, #5            // shifted_high = scaled_high >> 5
    
    add v16.4s, v16.4s, v12.4s         // result_low = shifted_low + bg_low
    add v17.4s, v17.4s, v13.4s         // result_high = shifted_high + bg_high
    
    // Apply final mask
    mov w0, #0xF81F                    // Low part of mask
    movk w0, #0x07E0, lsl #16          // High part of mask  
    dup v18.4s, w0                     // Broadcast to vector
    and v16.16b, v16.16b, v18.16b      // result_low &= mask
    and v17.16b, v17.16b, v18.16b      // result_high &= mask
    
    // Convert back to 16-bit: (result >> 16) | result
    ushr v18.4s, v16.4s, #16          // temp = result_low >> 16
    orr v16.16b, v16.16b, v18.16b      // final_low = result_low | temp
    ushr v18.4s, v17.4s, #16          // temp = result_high >> 16
    orr v17.16b, v17.16b, v18.16b      // final_high = result_high | temp
    
    // Pack back to 16-bit
    xtn v18.4h, v16.4s                 // packed_low = narrow final_low
    xtn2 v18.8h, v17.4s                // packed_high = narrow final_high (combine with low)
    
    // Apply conditional masks using BSL (bit select)
    // result = mix_zero_mask ? c2_vec : result
    bsl v3.16b, v1.16b, v18.16b        // v3 = mix_zero_mask ? c2_vec : result
    
    // result = mix_full_mask ? c1_vec : result  
    bsl v4.16b, v0.16b, v3.16b         // v4 = mix_full_mask ? c1_vec : previous_result
    
    // result = equal_mask ? c1_vec : result
    bsl v5.16b, v0.16b, v4.16b         // v5 = equal_mask ? c1_vec : previous_result
    
    // Move final result to return register
    mov v0.16b, v5.16b                 // return result in v0
    
    ret

.size lv_color_16_16_mix_8_asm, .-lv_color_16_16_mix_8_asm
