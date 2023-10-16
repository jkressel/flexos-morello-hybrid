
#define TEST_NAME "scalarmult8"
#include "cmptest.h"

typedef struct TestData_ {
    const char  pk_hex[crypto_scalarmult_BYTES * 2 + 1];
    const char  sk_hex[crypto_scalarmult_SCALARBYTES * 2 + 1];
    const char  shared_hex[crypto_scalarmult_BYTES * 2 + 1];
    const char *outcome;
} TestData;

static TestData test_data[] = {
    {
        "9c647d9ae589b9f58fdc3ca4947efbc915c4b2e08e744a0edf469dac59c8f85a",
        "4852834d9d6b77dadeabaaf2e11dca66d19fe74993a7bec36c6e16a0983feaba",
        "87b7f212b627f7a54ca5e0bcdaddd5389d9de6156cdbcf8ebe14ffbcfb436551",
        "valid"
    },
    {
        "9c647d9ae589b9f58fdc3ca4947efbc915c4b2e08e744a0edf469dac59c8f85a",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "4b82bd8650ea9b81a42181840926a4ffa16434d1bf298de1db87efb5b0a9e34e",
        "valid"
    },
    {
        "63aa40c6e38346c5caf23a6df0a5e6c80889a08647e551b3563449befcfc9733",
        "588c061a50804ac488ad774ac716c3f5ba714b2712e048491379a500211998a8",
        "b1a707519495ffffb298ff941716b06dfab87cf8d91123fe2be9a233dda22212",
        "acceptable"
    },
    {
        "0f83c36fded9d32fadf4efa3ae93a90bb5cfa66893bc412c43fa7287dbb99779",
        "b05bfd32e55325d9fd648cb302848039000b390e44d521e58aab3b29a6960ba8",
        "67dd4a6e165533534c0e3f172e4ab8576bca923a5f07b2c069b4c310ff2e935b",
        "acceptable"
    },
    {
        "0b8211a2b6049097f6871c6c052d3c5fc1ba17da9e32ae458403b05bb283092a",
        "70e34bcbe1f47fbc0fddfd7c1e1aa53d57bfe0f66d243067b424bb6210bed19c",
        "4a0638cfaa9ef1933b47f8939296a6b25be541ef7f70e844c0bcc00b134de64a",
        "acceptable"
    },
    {
        "343ac20a3b9c6a27b1008176509ad30735856ec1c8d8fcae13912d08d152f46c",
        "68c1f3a653a4cdb1d37bba94738f8b957a57beb24d646e994dc29a276aad458d",
        "399491fce8dfab73b4f9f611de8ea0b27b28f85994250b0f475d585d042ac207",
        "acceptable"
    },
    {
        "fa695fc7be8d1be5bf704898f388c452bafdd3b8eae805f8681a8d15c2d4e142",
        "d877b26d06dff9d9f7fd4c5b3769f8cdd5b30516a5ab806be324ff3eb69ea0b2",
        "2c4fe11d490a53861776b13b4354abd4cf5a97699db6e6c68c1626d07662f758",
        "acceptable"
    },
    {
        "0000000000000000000000000000000000000000000000000000000000000000",
        "207494038f2bb811d47805bcdf04a2ac585ada7f2f23389bfd4658f9ddd4debc",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "0100000000000000000000000000000000000000000000000000000000000000",
        "202e8972b61c7e61930eb9450b5070eae1c670475685541f0476217e4818cfab",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "0200000000000000000000000000000000000000000000000000000000000000",
        "38dde9f3e7b799045f9ac3793d4a9277dadeadc41bec0290f81f744f73775f84",
        "9a2cfe84ff9c4a9739625cae4a3b82a906877a441946f8d7b3d795fe8f5d1639",
        "acceptable"
    },
    {
        "0300000000000000000000000000000000000000000000000000000000000000",
        "9857a914e3c29036fd9a442ba526b5cdcdf28216153e636c10677acab6bd6aa5",
        "4da4e0aa072c232ee2f0fa4e519ae50b52c1edd08a534d4ef346c2e106d21d60",
        "acceptable"
    },
    {
        "ffffff030000f8ffff1f0000c0ffffff000000feffff070000f0ffff3f000000",
        "48e2130d723305ed05e6e5894d398a5e33367a8c6aac8fcdf0a88e4b42820db7",
        "9ed10c53747f647f82f45125d3de15a1e6b824496ab40410ffcc3cfe95760f3b",
        "acceptable"
    },
    {
        "000000fcffff070000e0ffff3f000000ffffff010000f8ffff0f0000c0ffff7f",
        "28f41011691851b3a62b641553b30d0dfddcb8fffcf53700a7be2f6a872e9fb0",
        "cf72b4aa6aa1c9f894f4165b86109aa468517648e1f0cc70e1ab08460176506b",
        "acceptable"
    },
    {
        "00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffff7f",
        "18a93b6499b9f6b3225ca02fef410e0adec23532321d2d8ef1a6d602a8c65b83",
        "5d50b62836bb69579410386cf7bb811c14bf85b1c7b17e5924c7ffea91ef9e12",
        "acceptable"
    },
    {
        "eaffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "c01d1305a1338a1fcac2ba7e2e032b427e0b04903165aca957d8d0553d8717b0",
        "19230eb148d5d67c3c22ab1daeff80a57eae4265ce2872657b2c8099fc698e50",
        "acceptable"
    },
    {
        "0400000000000000000000000000000000000000000000000000000000000000",
        "386f7f16c50731d64f82e6a170b142a4e34f31fd7768fcb8902925e7d1e21abe",
        "0fcab5d842a078d7a71fc59b57bfb4ca0be6873b49dcdb9f44e14ae8fbdfa542",
        "valid"
    },
    {
        "ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000",
        "e023a289bd5e90fa2804ddc019a05ef3e79d434bb6ea2f522ecb643a75296e95",
        "54ce8f2275c077e3b1306a3939c5e03eef6bbb88060544758d9fef59b0bc3e4f",
        "valid"
    },
    {
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff03",
        "68f010d62ee8d926053a361c3a75c6ea4ebdc8606ab285003a6f8f4076b01e83",
        "f136775c5beb0af8110af10b20372332043cab752419678775a223df57c9d30d",
        "valid"
    },
    {
        "fffffffbfffffbffffdfffffdffffffffefffffefffff7fffff7ffffbfffff3f",
        "58ebcb35b0f8845caf1ec630f96576b62c4b7b6c36b29deb2cb0084651755c96",
        "bf9affd06b844085586460962ef2146ff3d4533d9444aab006eb88cc3054407d",
        "valid"
    },
    {
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff3f",
        "188c4bc5b9c44b38bb658b9b2ae82d5b01015e093184b17cb7863503a783e1bb",
        "d480de04f699cb3be0684a9cc2e31281ea0bc5a9dcc157d3d20158d46ca5246d",
        "valid"
    },
    {
        "fffffffffeffff7ffffffffffeffff7ffffffffffeffff7ffffffffffeffff7f",
        "e06c11bb2e13ce3dc7673f67f5482242909423a9ae95ee986a988d98faee23a2",
        "4c4401cce6b51e4cb18f2790246c9bf914db667750a1cb89069092af07292276",
        "valid"
    },
    {
        "ebffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "c0658c46dde18129293877535b1162b6f9f5414a23cf4d2cbc140a4d99da2b8f",
        "578ba8cc2dbdc575afcf9df2b3ee6189f5337d6854c79b4ce165ea12293b3a0f",
        "valid"
    },
    {
        "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800",
        "10255c9230a97a30a458ca284a629669293a31890cda9d147febc7d1e22d6bb1",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f1157",
        "78f1e8edf14481b389448dac8f59c70b038e7cf92ef2c7eff57a72466e115296",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "a0a05a3e8f9f44204d5f8059a94ac7dfc39a49ac016dd743dbfa43c5d671fd88",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "d0dbb3ed1906663f15420af31f4eaf6509d9a9949723500605ad7c1c6e7450a9",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "c0b1d0eb22b244fe3291140072cdd9d989b5f0ecd96c100feb5bca241c1d9f8f",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "0000000000000000000000000000000000000000000000000000000000000080",
        "480bf45f594942a8bc0f3353c6e8b8853d77f351f1c2ca6c2d1abf8a00b4229c",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "0100000000000000000000000000000000000000000000000000000000000080",
        "30f993fcf8514fc89bd8db14cd43ba0d4b2530e73c4276a05e1b145d420cedb4",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b880",
        "c04974b758380e2a5b5df6eb09bb2f6b3434f982722a8e676d3da251d1b3de83",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f11d7",
        "502a31373db32446842fe5add3e024022ea54f274182afc3d9f1bb3d39534eb5",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "90fa6417b0e37030fd6e43eff2abaef14c6793117a039cf621318ba90f4e98be",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "78ad3f26027f1c9fdd975a1613b947779bad2cf2b741ade01840885a30bb979c",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "98e23de7b1e0926ed9c87e7b14baf55f497a1d7096f93977680e44dc1c7b7b8b",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "0000000000000000000000000000000000000000000000000000000000000000",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "0100000000000000000000000000000000000000000000000000000000000000",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f1157",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "0000000000000000000000000000000000000000000000000000000000000080",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "0100000000000000000000000000000000000000000000000000000000000080",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f11d7",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b880",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "1064a67da639a8f6df4fbea2d63358b65bca80a770712e14ea8a72df5a3313ae",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "efffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "f01e48dafac9d7bcf589cbc382c878d18bda3550589ffb5d50b523bebe329dae",
        "bd36a0790eb883098c988b21786773de0b3a4df162282cf110de18dd484ce74b",
        "acceptable"
    },
    {
        "f0ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "288796bc5aff4b81a37501757bc0753a3c21964790d38699308debc17a6eaf8d",
        "b4e0dd76da7b071728b61f856771aa356e57eda78a5b1655cc3820fb5f854c5c",
        "acceptable"
    },
    {
        "f1ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "98df845f6651bf1138221f119041f72b6dbc3c4ace7143d99fd55ad867480da8",
        "6fdf6c37611dbd5304dc0f2eb7c9517eb3c50e12fd050ac6dec27071d4bfc034",
        "acceptable"
    },
    {
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "f09498e46f02f878829e78b803d316a2ed695d0498a08abdf8276930e24edcb0",
        "4c8fc4b1c6ab88fb21f18f6d4c810240d4e94651ba44f7a2c863cec7dc56602d",
        "acceptable"
    },
    {
        "0200000000000000000000000000000000000000000000000000000000000080",
        "1813c10a5c7f21f96e17f288c0cc37607c04c5f5aea2db134f9e2ffc66bd9db8",
        "1cd0b28267dc541c642d6d7dca44a8b38a63736eef5c4e6501ffbbb1780c033c",
        "acceptable"
    },
    {
        "0300000000000000000000000000000000000000000000000000000000000080",
        "7857fb808653645a0beb138a64f5f4d733a45ea84c3cda11a9c06f7e7139149e",
        "8755be01c60a7e825cff3e0e78cb3aa4333861516aa59b1c51a8b2a543dfa822",
        "acceptable"
    },
    {
        "0400000000000000000000000000000000000000000000000000000000000080",
        "e03aa842e2abc56e81e87b8b9f417b2a1e5913c723eed28d752f8d47a59f498f",
        "54c9a1ed95e546d27822a360931dda60a1df049da6f904253c0612bbdc087476",
        "acceptable"
    },
    {
        "daffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "f8f707b7999b18cb0d6b96124f2045972ca274bfc154ad0c87038c24c6d0d4b2",
        "cc1f40d743cdc2230e1043daba8b75e810f1fbab7f255269bd9ebb29e6bf494f",
        "acceptable"
    },
    {
        "dbffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "a034f684fa631e1a348118c1ce4c98231f2d9eec9ba5365b4a05d69a785b0796",
        "54998ee43a5b007bf499f078e736524400a8b5c7e9b9b43771748c7cdf880412",
        "acceptable"
    },
    {
        "dcffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "30b6c6a0f2ffa680768f992ba89e152d5bc9893d38c9119be4f767bfab6e0ca5",
        "ead9b38efdd723637934e55ab717a7ae09eb86a21dc36a3feeb88b759e391e09",
        "acceptable"
    },
    {
        "eaffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "901b9dcf881e01e027575035d40b43bdc1c5242e030847495b0c7286469b6591",
        "602ff40789b54b41805915fe2a6221f07a50ffc2c3fc94cf61f13d7904e88e0e",
        "acceptable"
    },
    {
        "ebffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "8046677c28fd82c9a1bdb71a1a1a34faba1225e2507fe3f54d10bd5b0d865f8e",
        "e00ae8b143471247ba24f12c885536c3cb981b58e1e56b2baf35c12ae1f79c26",
        "acceptable"
    },
    {
        "efffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "602f7e2f68a846b82cc269b1d48e939886ae54fd636c1fe074d710127d472491",
        "98cb9b50dd3fc2b0d4f2d2bf7c5cfdd10c8fcd31fc40af1ad44f47c131376362",
        "acceptable"
    },
    {
        "f0ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "60887b3dc72443026ebedbbbb70665f42b87add1440e7768fbd7e8e2ce5f639d",
        "38d6304c4a7e6d9f7959334fb5245bd2c754525d4c91db950206926234c1f633",
        "acceptable"
    },
    {
        "f1ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "78d31dfa854497d72d8def8a1b7fb006cec2d8c4924647c93814ae56faeda495",
        "786cd54996f014a5a031ec14db812ed08355061fdb5de680a800ac521f318e23",
        "acceptable"
    },
    {
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "c04c5baefa8302ddded6a4bb957761b4eb97aefa4fc3b8043085f96a5659b3a5",
        "29ae8bc73e9b10a08b4f681c43c3e0ac1a171d31b38f1a48efba29ae639ea134",
        "acceptable"
    },
    {
        "e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c",
        "a046e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449a44",
        "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552",
        "valid"
    },
    {
        "e5210f12786811d3f4b7959d0538ae2c31dbe7106fc03c3efc4cd549c715a413",
        "4866e9d4d1b4673c5ad22691957d6af5c11b6421e0ea01d42ca4169e7918ba4d",
        "95cbde9476e8907d7aade45cb4b873f88b595a68799fa152e6f8f7647aac7957",
        "valid"
    },
    {
        "0ab4e76380d84dde4f6833c58f2a9fb8f83bb0169b172be4b6e0592887741a36",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "0200000000000000000000000000000000000000000000000000000000000000",
        "acceptable"
    },
    {
        "89e10d5701b4337d2d032181538b1064bd4084401ceca1fd12663a1959388000",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "0900000000000000000000000000000000000000000000000000000000000000",
        "valid"
    },
    {
        "2b55d3aa4a8f80c8c0b2ae5f933e85af49beac36c2fa7394bab76c8933f8f81d",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "1000000000000000000000000000000000000000000000000000000000000000",
        "valid"
    },
    {
        "63e5b1fe9601fe84385d8866b0421262f78fbfa5aff9585e626679b18547d959",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "feffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff3f",
        "acceptable"
    },
    {
        "e428f3dac17809f827a522ce32355058d07369364aa78902ee10139b9f9dd653",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "fcffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff3f",
        "valid"
    },
    {
        "b3b50e3ed3a407b95de942ef74575b5ab8a10c09ee103544d60bdfed8138ab2b",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "f9ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff3f",
        "acceptable"
    },
    {
        "213fffe93d5ea8cd242e462844029922c43c77c9e3e42f562f485d24c501a20b",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "f3ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff3f",
        "valid"
    },
    {
        "91b232a178b3cd530932441e6139418f72172292f1da4c1834fc5ebfefb51e3f",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff03",
        "valid"
    },
    {
        "045c6e11c5d332556c7822fe94ebf89b56a3878dc27ca079103058849fabcb4f",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "e5ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "acceptable"
    },
    {
        "1ca2190b71163539063c35773bda0c9c928e9136f0620aeb093f099197b7f74e",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "e3ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "acceptable"
    },
    {
        "f76e9010ac33c5043b2d3b76a842171000c4916222e9e85897a0aec7f6350b3c",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "ddffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "valid"
    },
    {
        "bb72688d8f8aa7a39cd6060cd5c8093cdec6fe341937c3886a99346cd07faa55",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "dbffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
        "acceptable"
    },
    {
        "88fddea193391c6a5933ef9b71901549447205aae9da928a6b91a352ba10f41f",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "0000000000000000000000000000000000000000000000000000000000000002",
        "acceptable"
    },
    {
        "303b392f153116cad9cc682a00ccc44c95ff0d3bbe568beb6c4e739bafdc2c68",
        "a0a4f130b98a5be4b1cedb7cb85584a3520e142d474dc9ccb909a073a976bf63",
        "0000000000000000000000000000000000000000000000000000000000008000",
        "acceptable"
    },
    {
        "fd300aeb40e1fa582518412b49b208a7842b1e1f056a040178ea4141534f652d",
        "c81724704000b26d31703cc97e3a378d56fad8219361c88cca8bd7c5719b12b2",
        "b734105dc257585d73b566ccb76f062795ccbec89128e52b02f3e59639f13c46",
        "valid"
    },
    {
        "c8ef79b514d7682677bc7931e06ee5c27c9b392b4ae9484473f554e6678ecc2e",
        "c81724704000b26d31703cc97e3a378d56fad8219361c88cca8bd7c5719b12b2",
        "647a46b6fc3f40d62141ee3cee706b4d7a9271593a7b143e8e2e2279883e4550",
        "valid"
    },
    {
        "64aeac2504144861532b7bbcb6c87d67dd4c1f07ebc2e06effb95aecc6170b2c",
        "c81724704000b26d31703cc97e3a378d56fad8219361c88cca8bd7c5719b12b2",
        "4ff03d5fb43cd8657a3cf37c138cadcecce509e4eba089d0ef40b4e4fb946155",
        "valid"
    },
    {
        "bf68e35e9bdb7eee1b50570221860f5dcdad8acbab031b14974cc49013c49831",
        "c81724704000b26d31703cc97e3a378d56fad8219361c88cca8bd7c5719b12b2",
        "21cee52efdbc812e1d021a4af1e1d8bc4db3c400e4d2a2c56a3926db4d99c65b",
        "valid"
    },
    {
        "5347c491331a64b43ddc683034e677f53dc32b52a52a577c15a83bf298e99f19",
        "c81724704000b26d31703cc97e3a378d56fad8219361c88cca8bd7c5719b12b2",
        "18cb89e4e20c0c2bd324305245266c9327690bbe79acb88f5b8fb3f74eca3e52",
        "valid"
    },
    {
        "258e04523b8d253ee65719fc6906c657192d80717edc828fa0af21686e2faa75",
        "a023cdd083ef5bb82f10d62e59e15a6800000000000000000000000000000050",
        "258e04523b8d253ee65719fc6906c657192d80717edc828fa0af21686e2faa75",
        "valid"
    },
    {
        "2eae5ec3dd494e9f2d37d258f873a8e6e9d0dbd1e383ef64d98bb91b3e0be035",
        "58083dd261ad91eff952322ec824c682ffffffffffffffffffffffffffffff5f",
        "2eae5ec3dd494e9f2d37d258f873a8e6e9d0dbd1e383ef64d98bb91b3e0be035",
        "acceptable"
    }
};

int
scalarmult8_xmain(void)
{
    unsigned char sk[crypto_scalarmult_SCALARBYTES];
    unsigned char pk[crypto_scalarmult_BYTES];
    unsigned char shared[crypto_scalarmult_BYTES];
    unsigned char shared2[crypto_scalarmult_BYTES];
    unsigned int  i;
    int           res;

    for (i = 0U; i < (sizeof test_data) / (sizeof test_data[0]); i++) {
        sodium_hex2bin(sk, crypto_scalarmult_SCALARBYTES, test_data[i].sk_hex,
                       crypto_scalarmult_SCALARBYTES * 2, NULL, NULL, NULL);
        sodium_hex2bin(pk, crypto_scalarmult_BYTES, test_data[i].pk_hex,
                       crypto_scalarmult_BYTES * 2, NULL, NULL, NULL);
        sodium_hex2bin(shared, crypto_scalarmult_BYTES, test_data[i].shared_hex,
                       crypto_scalarmult_BYTES * 2, NULL, NULL, NULL);
        randombytes_buf(shared2, crypto_scalarmult_BYTES);
        res = crypto_scalarmult(shared2, sk, pk);
        if (res == 0) {
            if (strcmp(test_data[i].outcome, "acceptable") == 0) {
                printf("test case %u succeeded (%s)\n", i,
                       test_data[i].outcome);
            } else if (strcmp(test_data[i].outcome, "valid") != 0) {
                printf("*** test case %u succeeded, was supposed to be %s\n", i,
                       test_data[i].outcome);
            }
            if (memcmp(shared, shared2, crypto_scalarmult_BYTES) != 0) {
                printf("*** test case %u succeeded, but shared key is not %s\n",
                       i, test_data[i].outcome);
            }
        } else {
            if (strcmp(test_data[i].outcome, "acceptable") == 0) {
                printf("test case %u failed (%s)\n", i, test_data[i].outcome);
            } else if (strcmp(test_data[i].outcome, "valid") == 0) {
                printf("*** test case %u failed, was supposed to be %s\n", i,
                       test_data[i].outcome);
            }
        }
    }
    printf("OK\n");

    return 0;
}
