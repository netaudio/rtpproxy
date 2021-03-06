#!/bin/sh

set -e

TDIR="`dirname "${0}"`"
. "${TDIR}/genfincode.sub"

gen_fin_c() {
  echo "/* Auto-generated by ${GENRNAME} - DO NOT EDIT! */"
  echo "#include <stdio.h>"
  echo "#include <stdint.h>"
  echo "#include <stdlib.h>"
  echo "#include \"rtpp_types.h\""
  echo "#include \"rtpp_debug.h\""
  echo "#include \"${1}\""
  echo "#include \"${2}\""

  for oname in ${ONAMES}
  do
    MNAMES=`get_mnames "${1}" "${oname}"`
    for mname in ${MNAMES}
    do
      epname=`get_epname "${1}" "${mname}"`
      emit_finfunction "${mname}" "${epname}" "${oname}"
    done
    echo "static const struct ${oname}_smethods ${oname}_smethods_fin = {"
    for mname in ${MNAMES}
    do
      epname=`get_epname "${1}" "${mname}"`
      echo "    .${epname} = (${mname}_t)&${mname}_fin,"
    done
    echo "};"
  done

  smname=smethods
  for oname in ${ONAMES}
  do
    echo   "void ${oname}_fin(struct ${oname} *pub) {"
    MNAMES=`get_mnames ${1} ${oname}`
    for mname in ${MNAMES}
    do
      epname=`get_epname "${1}" "${mname}"`
      echo "    RTPP_DBG_ASSERT(pub->${smname}->${epname} != (${mname}_t)NULL);"
    done
    echo   "    RTPP_DBG_ASSERT(pub->${smname} != &${oname}_${smname}_fin &&"
    echo   "      pub->${smname} != NULL);"
    echo   "    pub->${smname} = &${oname}_${smname}_fin;"
    echo   "}"
  done
  emit_fintestsection ${1} "${ONAMES}" 1
}

hname=`basename "${2}"`
emit_fin_h "${1}" "${hname}" > "${2}"
gen_fin_c "${1}" "${hname}" > "${3}"
