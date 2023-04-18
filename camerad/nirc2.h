#ifndef NIRC2_H
#define NIRC2_H
namespace Archon {

      const int SAMPMODE_SINGLE = 1;  const std::string SAMPSTR_SINGLE = "SINGLE";
      const int SAMPMODE_CDS    = 2;  const std::string SAMPSTR_CDS    = "CDS";
      const int SAMPMODE_MCDS   = 3;  const std::string SAMPSTR_MCDS   = "MCDS";
      const int SAMPMODE_UTR    = 4;  const std::string SAMPSTR_UTR    = "UTR";
      const int SAMPMODE_RXV    = 5;  const std::string SAMPSTR_RXV    = "RXV";
      const int SAMPMODE_RXRV   = 6;  const std::string SAMPSTR_RXRV   = "RXRV";

      const std::vector<std::string> SAMPMODE_NAME { SAMPSTR_SINGLE,
                                                     SAMPSTR_CDS,
                                                     SAMPSTR_MCDS,
                                                     SAMPSTR_UTR,
                                                     SAMPSTR_RXV,
                                                     SAMPSTR_RXRV,
                                                   };
}
#endif
