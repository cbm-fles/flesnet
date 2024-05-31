#!/bin/bash

mstool="../../../build/mstool"
flesnet="../../../build/flesnet"
tsclient="../../../build/tsclient"
archive_validator="../../../build/archive_validator"

# Checks if the given argument is a integer
# $1 - number to check
function is_number() {
    case $1 in
        ''|*[!0-9]*) echo 0;; # no - is not a number
        *) echo 1 ;; # yes - is number
    esac
}


# $1 - number of entry nodes
# $2 - number of build nodes
# $3 - number of timeslices
# $4 - timeslice size
# $5 - overlap size
function test_chain() {
    if [ $# -ne 5 ]; then
        return 1;
    fi
    
    for arg in "$@"
    do
        local num_check=$(is_number $arg)
        if [ $num_check -ne 1 ]; then
            return 1
        fi
    done

    local entry_cnt=$1
    local build_cnt=$2
    local timeslice_cnt=$3
    local timeslice_size=$4
    local overlap_size=$5
    local dir_name="$1to$2_test"
    local microslice_cnt=$(($timeslice_size * $timeslice_cnt + $overlap_size))
    rm -rf ./$dir_name
    mkdir ./$dir_name
    echo $timeslice_size
    # Use mstool to create input microslice archives
    for i in $(seq 0 $(($entry_cnt - 1)));
    do
        $mstool -p 0 -n $microslice_cnt -c $i -o "./$dir_name/input$i.msa"
        if [ $? -ne 0 ]; then
            return 1;
        fi
    done

    # Use mstool to provide the created microslice archives via shared memory
    #./mstool -c 1 --input-archive ./ms_archive1.msa --output-shm fles_in_shared_memory1
    local mstool_ids=()
    for i in $(seq 0 $(($entry_cnt - 1)));
    do
        rm -rf /dev/shm/fles_in_shared_memory$i
        $mstool -c $i --input-archive "./$dir_name/input$i.msa" --output-shm fles_in_shared_memory$i > /dev/null 2>&1 &
        mstool_ids+=($!)
    done

    # ./flesnet -n 15 -t zeromq -i 1 -I shm:/fles_in_shared_memory0/0 shm:/fles_in_shared_memory1/0 -O shm:/fles_out_shared_memory0/0 shm:/fles_out_shared_memory1/0 --processor-instances 1 -e "./tsclient -i shm:%s -o file:timeslice_archive.tsa"
    local entry_input_vector_arg="" # -I arg for entry nodes
    # ./flesnet -n 15 -t zeromq -o 1 -I shm://127.0.0.1/0 shm://127.0.0.1/0 -O shm:/fles_out_shared_memory0/0 shm:/fles_out_shared_memory1/1 --processor-instances 1 -e "./tsclient -i shm:%s -o file:timeslice_archive1.tsa"
    local build_input_vector_arg="" # -I arg for build nodes
    for i in $(seq 0 $(($entry_cnt  - 1)));
    do
        entry_input_vector_arg=" $entry_input_vector_arg shm:/fles_in_shared_memory$i/0?overlap=$overlap_size"
        build_input_vector_arg=" $build_input_vector_arg shm://127.0.0.1/0?overlap=$overlap_size"
    done
    
    local output_vector_arg="" # -O arg for build nodes
    for i in $(seq 0 $(($build_cnt  - 1)));
    do
        output_vector_arg=" $output_vector_arg shm:/fles_out_shared_memory$i/0?overlap=$overlap_size"
    done

    local build_ids=()
    # Start build nodes
    for i in $(seq 0 $(($build_cnt - 1)));
    do
        local processor_executable="$tsclient --input-uri shm:%s -o file:./$dir_name/output$i.tsa"
        $flesnet -n $timeslice_cnt --timeslice-size $timeslice_size -o $i -I $build_input_vector_arg -O $output_vector_arg --processor-instances 1 -e "$processor_executable" -t zeromq > /dev/null 2>&1 &
        build_ids+=($!)
    done
    
    local entry_ids=()
    # Start entry nodes
    for i in $(seq 0 $(($entry_cnt - 1)));
    do
        local processor_executable="$tsclient --input-uri shm:%s -o file:./$dir_name/output$i.tsa"
        $flesnet -n $timeslice_cnt --timeslice-size $timeslice_size -i $i -I $entry_input_vector_arg -O $output_vector_arg -t zeromq --processor-instances 1 -e "$processor_executable" > /dev/null 2>&1 &
        entry_ids+=($!)
    done

    wait ${build_ids[*]}
    wait ${entry_ids[*]}
    for id in "${mstool_ids[@]}";
    do
        kill -9 $id > /dev/null 2>&1
    done

    $archive_validator -I $(find ./$dir_name -name "*.msa") -O $(find ./$dir_name -name "*.tsa") --timeslice-size $timeslice_size --timeslice-cnt $timeslice_cnt --overlap $overlap_size
    return $?
}
        
test_chain 1 1 15 200 1
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi

test_chain 1 1 15 200 2
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi

test_chain 3 2 15 100 1
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi

test_chain 3 2 15 200 1
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi


echo -e "\e[1;37;1;42mTests successful\e[0m"
exit 0